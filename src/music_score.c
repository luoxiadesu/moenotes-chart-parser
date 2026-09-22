#include "music_score.h"
#include "yyjson.h"
#include <errno.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#define PPQ 480
#define LANES 24
#define EPSILON (8.0 * 1.1920928955078125e-7)

typedef struct note {
    ms_note_view_t v;
    char type[8];
    char direction[8];
    char ease[16];
    int line_index;
} note_t;

typedef struct bpm {
    int32_t tick;
    int32_t time_ms;
    double value;
} bpm_t;

typedef struct sig {
    int32_t tick, bar, ticks_per_bar, numerator, denominator;
} sig_t;

struct ms_score {
    ms_allocator_t alc;
    note_t *notes;
    size_t note_count, note_cap;
    bpm_t *bpms;
    size_t bpm_count, bpm_cap;
    sig_t *sigs;
    size_t sig_count, sig_cap;
    int32_t next_id;
    int32_t lane_count;
    uint8_t mirror;
};

static void *std_malloc(void *ctx, size_t n) { (void)ctx; return malloc(n); }
static void *std_realloc(void *ctx, void *p, size_t n) { (void)ctx; return realloc(p, n); }
static void std_free(void *ctx, void *p) { (void)ctx; free(p); }
static const ms_allocator_t DEFAULT_ALC = { NULL, std_malloc, std_realloc, std_free };

static void set_error(char *dst, size_t cap, const char *msg) {
    if (dst && cap) { snprintf(dst, cap, "%s", msg ? msg : "error"); }
}
static int valid_alc(const ms_allocator_t *a) {
    return a && a->malloc_fn && a->realloc_fn && a->free_fn;
}
static void *a_malloc(const ms_allocator_t *a, size_t n) { return a->malloc_fn(a->ctx, n); }
static void *a_realloc(const ms_allocator_t *a, void *p, size_t n) { return a->realloc_fn(a->ctx, p, n); }
static void a_free(const ms_allocator_t *a, void *p) { if (p) a->free_fn(a->ctx, p); }

static void *yy_malloc(void *ctx, size_t n) { return a_malloc((const ms_allocator_t *)ctx, n); }
static void *yy_realloc(void *ctx, void *p, size_t old_n, size_t n) {
    (void)old_n; return a_realloc((const ms_allocator_t *)ctx, p, n);
}
static void yy_free(void *ctx, void *p) { a_free((const ms_allocator_t *)ctx, p); }

static int grow(const ms_allocator_t *a, void **ptr, size_t *cap, size_t count, size_t elem) {
    if (count < *cap) return 1;
    size_t nc = *cap ? *cap * 2 : 64;
    if (nc < count || nc > SIZE_MAX / elem) return 0;
    void *p = a_realloc(a, *ptr, nc * elem);
    if (!p) return 0;
    *ptr = p; *cap = nc; return 1;
}

void ms_default_parse_options(ms_parse_options_t *o) {
    if (!o) return;
    o->start_note_id = 0; o->slide_combo_unit = 0; o->mirror = 0; o->add_flick_hidden = 0;
}

const char *ms_result_string(ms_result_t r) {
    switch (r) {
    case MS_OK: return "ok"; case MS_ERR_INVALID_ARGUMENT: return "invalid argument";
    case MS_ERR_OUT_OF_MEMORY: return "out of memory"; case MS_ERR_GZIP: return "gzip error";
    case MS_ERR_JSON: return "invalid json"; case MS_ERR_SCHEMA: return "schema error";
    case MS_ERR_RANGE: return "range error"; default: return "internal error";
    }
}

const char *ms_operate_type_name(ms_operate_type_t t) {
    switch (t) {
    case MS_OP_NORMAL: return "normal"; case MS_OP_SLIDE_BEGIN: return "slide_begin";
    case MS_OP_SLIDE_CONNECTION: return "slide_connection"; case MS_OP_SLIDE_END: return "slide_end";
    case MS_OP_FLICK: return "flick"; case MS_OP_SLIDE_BEGIN_FLICK: return "slide_begin_flick";
    case MS_OP_SLIDE_END_FLICK: return "slide_end_flick"; case MS_OP_TRACE: return "trace";
    case MS_OP_SLIDE_BEGIN_TRACE: return "slide_begin_trace"; case MS_OP_SLIDE_END_TRACE: return "slide_end_trace";
    case MS_OP_SLIDE_CONNECTION_TRACE: return "slide_connection_trace";
    case MS_OP_HIDDEN_SLIDE_BEGIN: return "hidden_slide_begin"; case MS_OP_HIDDEN_SLIDE_END: return "hidden_slide_end";
    case MS_OP_GUIDE_BEGIN: return "guide_begin"; case MS_OP_GUIDE_BEGIN_NORMAL: return "guide_begin_normal";
    case MS_OP_GUIDE_BEGIN_FLICK: return "guide_begin_flick"; case MS_OP_GUIDE_END: return "guide_end";
    case MS_OP_GUIDE_BEGIN_TRACE: return "guide_begin_trace"; case MS_OP_GUIDE_END_TRACE: return "guide_end_trace";
    case MS_OP_COMBO: return "combo"; case MS_OP_COMBO_SKIP: return "combo_skip";
    case MS_OP_HIDDEN: return "hidden"; case MS_OP_INVALID_HIDDEN: return "invalid_hidden";
    default: return "none";
    }
}

static int32_t json_i(yyjson_val *v, int32_t d) {
    if (!v || !yyjson_is_num(v)) return d;
    double x = yyjson_get_num(v);
    if (!isfinite(x)) return d;
    if (x >= (double)INT32_MAX) return INT32_MAX;
    if (x <= (double)INT32_MIN) return INT32_MIN;
    return (int32_t)x;
}
static double json_d(yyjson_val *v, double d) {
    if (!v || !yyjson_is_num(v)) return d;
    double x = yyjson_get_num(v);
    return isfinite(x) ? x : d;
}
static int json_b(yyjson_val *v, int d) { return v && yyjson_is_bool(v) ? yyjson_get_bool(v) : d; }
static const char *json_s(yyjson_val *v, const char *d) { const char *s = v ? yyjson_get_str(v) : NULL; return s ? s : d; }
static yyjson_val *obj(yyjson_val *o, const char *key) { return o ? yyjson_obj_get(o, key) : NULL; }

static int position(const ms_score_t *s, int32_t tick, ms_position_t *out) {
    if (!s || !out || !s->sig_count) return 0;
    const sig_t *sg = &s->sigs[0];
    for (size_t i = 0; i < s->sig_count; i++) { if (s->sigs[i].tick > tick) break; sg = &s->sigs[i]; }
    int32_t delta = tick - sg->tick; if (delta < 0) delta = 0;
    int32_t bo = sg->ticks_per_bar ? delta / sg->ticks_per_bar : 0;
    int32_t in = sg->ticks_per_bar ? delta % sg->ticks_per_bar : 0;
    out->bar = sg->bar + bo; out->rhythm = in; out->rhythmic_unit = sg->ticks_per_bar;
    out->bar_progress = sg->ticks_per_bar ? (double)in / sg->ticks_per_bar : 0.0;
    const bpm_t *bp = &s->bpms[0];
    for (size_t i = 0; i < s->bpm_count; i++) { if (s->bpms[i].tick > tick) break; bp = &s->bpms[i]; }
    double ms = bp->time_ms + (double)(tick - bp->tick) * 60000.0 / (bp->value * PPQ);
    out->time_ms = (int32_t)floor(ms); return 1;
}

static int same_pos(const ms_position_t *a, const ms_position_t *b) {
    if (a->bar != b->bar) return 0;
    double scale = fmax(fabs(a->bar_progress), fabs(b->bar_progress));
    double tol = fmax(scale * 1e-6, EPSILON);
    return fabs(a->bar_progress - b->bar_progress) < tol;
}

static ms_operate_type_t single_op(const char *type, int visible) {
    if (!visible) return MS_OP_HIDDEN;
    if (!strcmp(type, "flick")) return MS_OP_FLICK;
    if (!strcmp(type, "trace")) return MS_OP_TRACE;
    return MS_OP_NORMAL;
}
static ms_operate_type_t line_op(const char *line_type, const char *node_type, size_t i, size_t n, int visible) {
    if (!visible) return i == 0 ? MS_OP_HIDDEN_SLIDE_BEGIN : (i + 1 == n ? MS_OP_HIDDEN_SLIDE_END : MS_OP_HIDDEN);
    int first = i == 0, last = i + 1 == n;
    if (!strcmp(line_type, "guide")) {
        if (first) { if (!strcmp(node_type, "flick")) return MS_OP_GUIDE_BEGIN_FLICK; if (!strcmp(node_type, "trace")) return MS_OP_GUIDE_BEGIN_TRACE; return MS_OP_GUIDE_BEGIN; }
        if (last && !strcmp(node_type, "trace")) return MS_OP_GUIDE_END_TRACE;
        return last ? MS_OP_GUIDE_END : MS_OP_SLIDE_CONNECTION;
    }
    if (first && !strcmp(node_type, "flick")) return MS_OP_SLIDE_BEGIN_FLICK;
    if (first && !strcmp(node_type, "trace")) return MS_OP_SLIDE_BEGIN_TRACE;
    if (last && !strcmp(node_type, "flick")) return MS_OP_SLIDE_END_FLICK;
    if (last && !strcmp(node_type, "trace")) return MS_OP_SLIDE_END_TRACE;
    if (!strcmp(node_type, "trace")) return MS_OP_SLIDE_CONNECTION_TRACE;
    return first ? MS_OP_SLIDE_BEGIN : (last ? MS_OP_SLIDE_END : MS_OP_SLIDE_CONNECTION);
}

static int add_note(ms_score_t *s, const note_t *n) {
    if (!grow(&s->alc, (void **)&s->notes, &s->note_cap, s->note_count + 1, sizeof(*n))) return 0;
    s->notes[s->note_count++] = *n; return 1;
}
static int parse_note_base(ms_score_t *s, note_t *n, yyjson_val *raw, const char *type,
                           const char *node_type, ms_operate_type_t op, int32_t tick,
                           double pos, double width, int visible, int line_id,
                           int source_index, int slide_along, int pos_auto) {
    memset(n, 0, sizeof(*n)); n->v.id = s->next_id++; n->v.operate_type = op; n->v.tick = tick;
    if (!position(s, tick, &n->v.position)) return 0;
    n->v.lane_count = LANES; n->v.width = width; n->v.critical = (uint8_t)json_b(obj(raw, "crit"), 0);
    n->v.visible = (uint8_t)visible; n->v.slide_along = (uint8_t)slide_along; n->v.pos_auto = (uint8_t)pos_auto;
    n->v.line_id = line_id; n->v.source_index = source_index; n->v.pair_note_id = -1; n->v.parent_note_id = -1; n->v.hidden_for_note_id = -1;
    if (s->mirror) pos = LANES - pos - width;
    n->v.lane_start_float = pos; n->v.lane_end_float = pos + width - 1.0;
    n->v.lane_start = (int32_t)floor(pos); n->v.lane_end = (int32_t)floor(n->v.lane_end_float);
    snprintf(n->type, sizeof(n->type), "%s", type ? type : "tap");
    snprintf(n->direction, sizeof(n->direction), "%s", json_s(obj(raw, "dir"), "normal"));
    if (s->mirror && !strcmp(n->direction, "left")) snprintf(n->direction, sizeof(n->direction), "right");
    else if (s->mirror && !strcmp(n->direction, "right")) snprintf(n->direction, sizeof(n->direction), "left");
    snprintf(n->ease, sizeof(n->ease), "%s", json_s(obj(raw, "ease"), json_s(obj(raw, "easeL"), "linear")));
    (void)node_type; return 1;
}

static int first_tick(yyjson_val *raw) {
    yyjson_val *t = obj(raw, "t"); if (t && yyjson_is_num(t)) return json_i(t, 0);
    yyjson_val *nodes = obj(raw, "node"); yyjson_val *v = nodes && yyjson_is_arr(nodes) ? yyjson_arr_get_first(nodes) : NULL;
    return json_i(obj(v, "t"), 0);
}
typedef struct raw_ref {
    yyjson_val *value;
    size_t source_index;
} raw_ref_t;
static int cmp_raw(const void *aa, const void *bb) {
    const raw_ref_t *ra = aa, *rb = bb;
    yyjson_val *a = ra->value, *b = rb->value;
    int ta = first_tick(a), tb = first_tick(b);
    if (ta != tb) return ta < tb ? -1 : 1;
    return ra->source_index < rb->source_index ? -1 : ra->source_index > rb->source_index;
}

static ms_result_t inflate_input(const ms_allocator_t *a, const void *data, size_t size, unsigned char **out, size_t *out_n) {
    if (size > UINT_MAX) return MS_ERR_RANGE;
    if (size < 2 || ((const unsigned char *)data)[0] != 0x1f || ((const unsigned char *)data)[1] != 0x8b) {
        if (size == SIZE_MAX) return MS_ERR_RANGE;
        unsigned char *p = a_malloc(a, size + 1); if (!p) return MS_ERR_OUT_OF_MEMORY;
        memcpy(p, data, size); p[size] = 0; *out = p; *out_n = size; return MS_OK;
    }
    if (size > (SIZE_MAX - 1024) / 4) return MS_ERR_RANGE;
    size_t cap = size * 4 + 1024; if (cap < 4096) cap = 4096;
    if (cap > UINT_MAX) cap = UINT_MAX;
    for (;;) {
        unsigned char *p = a_malloc(a, cap); if (!p) return MS_ERR_OUT_OF_MEMORY;
        z_stream stream; memset(&stream, 0, sizeof(stream));
        stream.next_in = (Bytef *)data; stream.avail_in = (uInt)size;
        stream.next_out = p; stream.avail_out = (uInt)cap;
        int z = inflateInit2(&stream, 16 + MAX_WBITS);
        if (z == Z_OK) { z = inflate(&stream, Z_FINISH); }
        uLongf n = (uLongf)stream.total_out;
        inflateEnd(&stream);
        if (z == Z_OK || z == Z_STREAM_END) { *out = p; *out_n = (size_t)n; return MS_OK; }
        a_free(a, p); if (z != Z_BUF_ERROR || cap >= UINT_MAX) return MS_ERR_GZIP;
        cap = cap > UINT_MAX / 2 ? UINT_MAX : cap * 2;
    }
}

static int build_events(ms_score_t *s, yyjson_val *events) {
    if (!events || !yyjson_is_obj(events)) return 0;
    yyjson_val *bp = obj(events, "bpm"); size_t i, max; yyjson_val *v;
    if (bp && yyjson_is_arr(bp)) yyjson_arr_foreach(bp, i, max, v) {
        if (!grow(&s->alc, (void **)&s->bpms, &s->bpm_cap, s->bpm_count + 1, sizeof(*s->bpms))) return 0;
        double bpm = json_d(obj(v,"bpm"),120.0);
        if (bpm <= 0.0) return 0;
        s->bpms[s->bpm_count++] = (bpm_t){json_i(obj(v,"t"),0), 0, bpm};
    }
    if (!s->bpm_count) { if (!grow(&s->alc,(void**)&s->bpms,&s->bpm_cap,1,sizeof(*s->bpms))) return 0; s->bpms[0]=(bpm_t){0,0,120}; s->bpm_count=1; }
    for (i=1;i<s->bpm_count;i++) { bpm_t x=s->bpms[i]; size_t j=i; while(j && s->bpms[j-1].tick>x.tick){s->bpms[j]=s->bpms[j-1];j--;} s->bpms[j]=x; }
    for (i=1;i<s->bpm_count;i++) s->bpms[i].time_ms=(int32_t)floor(s->bpms[i-1].time_ms+(s->bpms[i].tick-s->bpms[i-1].tick)*60000.0/(s->bpms[i-1].value*PPQ));
    yyjson_val *sg = obj(events,"sig");
    if (sg && yyjson_is_arr(sg)) yyjson_arr_foreach(sg,i,max,v) {
        if (!grow(&s->alc,(void**)&s->sigs,&s->sig_cap,s->sig_count+1,sizeof(*s->sigs))) return 0;
        /* Native tick conversion uses explicit numerator/denominator fields;
           compact sig arrays are retained as source metadata only. */
        int num=json_i(obj(v,"numerator"),4);
        int den=json_i(obj(v,"denominator"),4);
        if (num <= 0 || den <= 0 || num > INT_MAX / (PPQ * 4) || (PPQ * 4 * num) / den <= 0) return 0;
        s->sigs[s->sig_count++]=(sig_t){json_i(obj(v,"t"),0),0,PPQ*4*num/den,num,den};
    }
    if (!s->sig_count) { if(!grow(&s->alc,(void**)&s->sigs,&s->sig_cap,1,sizeof(*s->sigs)))return 0; s->sigs[0]=(sig_t){0,0,PPQ*4,4,4};s->sig_count=1; }
    for(i=1;i<s->sig_count;i++){sig_t x=s->sigs[i];size_t j=i;while(j&&s->sigs[j-1].tick>x.tick){s->sigs[j]=s->sigs[j-1];j--;}s->sigs[j]=x;}
    s->sigs[0].bar = 0;
    for(i=1;i<s->sig_count;i++) s->sigs[i].bar=s->sigs[i-1].bar+(s->sigs[i].tick-s->sigs[i-1].tick)/s->sigs[i-1].ticks_per_bar;
    return 1;
}

static int pair_candidate(ms_operate_type_t t);
static int parse_notes(ms_score_t *s, yyjson_val *notes, uint32_t combo_unit) {
    size_t count = yyjson_arr_size(notes); if (!count) return 1;
    raw_ref_t *raws = a_malloc(&s->alc, count * sizeof(*raws)); if (!raws) return 0;
    size_t i, max; yyjson_val *v; yyjson_arr_foreach(notes,i,max,v) { raws[i].value=v; raws[i].source_index=i; } qsort(raws,count,sizeof(*raws),cmp_raw);
    for (i = 0; i < count; i++) if (!yyjson_is_obj(raws[i].value)) { a_free(&s->alc, raws); return 0; }
    int32_t previous_id=-1; ms_position_t previous_pos={0}; int have_previous=0; int line_id=0;
    for(i=0;i<count;i++) {
        yyjson_val *raw=raws[i].value; int source_index=(int)raws[i].source_index; const char *typ=json_s(obj(raw,"type"),"tap");
        yyjson_val *nodes=obj(raw,"node"); int isline=!strcmp(typ,"long")||!strcmp(typ,"guide");
        size_t nn=(nodes&&yyjson_is_arr(nodes))?yyjson_arr_size(nodes):0; int outer=(obj(raw,"t")||obj(raw,"pos")||obj(raw,"size"));
        size_t total=nn+(outer?1:0); if(isline&&!total) total=1;
        if(!isline){int32_t tick=json_i(obj(raw,"t"),0); int auto_pos=0; yyjson_val *pv=obj(raw,"pos"); const char *ps=json_s(pv,NULL); double pos=ps?0:json_d(pv,0); if(ps&&!strcmp(ps,"auto"))auto_pos=1; double width=json_d(obj(raw,"size"),1); int vis=json_b(obj(raw,"visible"),1)&&strcmp(json_s(obj(raw,"alpha"),"none"),"fadeout"); note_t n; if(!parse_note_base(s,&n,raw,typ,typ,single_op(typ,vis),tick,pos,width,vis,-1,source_index,0,auto_pos)){a_free(&s->alc,raws);return 0;} if(have_previous&&same_pos(&n.v.position,&previous_pos)&&pair_candidate(n.v.operate_type)&&pair_candidate(s->notes[s->note_count-1].v.operate_type)){n.v.pair_note_id=previous_id;s->notes[s->note_count-1].v.pair_note_id=n.v.id;} if(pair_candidate(n.v.operate_type)){previous_id=n.v.id;previous_pos=n.v.position;have_previous=1;} if(!add_note(s,&n)){a_free(&s->alc,raws);return 0;} continue;}
        int32_t ltick=first_tick(raw); int lid=line_id++; for(size_t k=0;k<total;k++){yyjson_val *node=(outer&&k==0)?raw:yyjson_arr_get(nodes,k-(outer?1:0)); if(!node)node=raw; int32_t tick=json_i(obj(node,"t"),ltick); const char *nt=json_s(obj(node,"type"),k==0?typ:"node"); double pos=json_d(obj(node,"pos"),json_d(obj(raw,"pos"),0)); const char *pstr=json_s(obj(node,"pos"),NULL); int pa=pstr&&!strcmp(pstr,"auto"); double width=json_d(obj(node,"size"),json_d(obj(raw,"size"),1)); int vis=json_b(obj(node,"visible"),json_b(obj(raw,"visible"),1))&&strcmp(json_s(obj(node,"alpha"),json_s(obj(raw,"alpha"),"none")),"fadeout"); note_t n; if(!parse_note_base(s,&n,raw,typ,nt,line_op(typ,nt,k,total,vis),tick,pos,width,vis,lid,source_index,k>0,pa)){a_free(&s->alc,raws);return 0;} if(k==0)n.v.parent_note_id=n.v.id; else n.v.parent_note_id=s->notes[s->note_count-1-(size_t)(0)].v.parent_note_id; if(!add_note(s,&n)){a_free(&s->alc,raws);return 0;}}
    }
    a_free(&s->alc,raws); (void)combo_unit; return 1;
}

static int cmp_note(const void *aa,const void *bb){const note_t*a=aa,*b=bb;return a->v.tick!=b->v.tick?a->v.tick-b->v.tick:a->v.id-b->v.id;}
static int judgement(ms_operate_type_t t);
static int pair_candidate(ms_operate_type_t t) {
    return t == MS_OP_NORMAL || t == MS_OP_SLIDE_BEGIN || t == MS_OP_FLICK ||
           t == MS_OP_SLIDE_BEGIN_FLICK || t == MS_OP_SLIDE_END_FLICK;
}

static int add_flick_hidden(ms_score_t *s) {
    size_t base = s->note_count;
    for (size_t i = 0; i < base; i++) {
        note_t *flick = &s->notes[i];
        if (flick->v.operate_type != MS_OP_FLICK) continue;
        for (size_t b = 0; b < base; b++) {
            note_t *begin = &s->notes[b];
            if (begin->v.operate_type != MS_OP_SLIDE_BEGIN) continue;
            int line = begin->v.line_id;
            int32_t end_tick = begin->v.tick;
            for (size_t e = 0; e < base; e++) {
                if (s->notes[e].v.line_id == line &&
                    (s->notes[e].v.operate_type == MS_OP_SLIDE_END ||
                     s->notes[e].v.operate_type == MS_OP_SLIDE_END_FLICK ||
                     s->notes[e].v.operate_type == MS_OP_SLIDE_END_TRACE)) {
                    end_tick = s->notes[e].v.tick;
                    break;
                }
            }
            if (flick->v.tick < begin->v.tick || flick->v.tick > end_tick) continue;
            int duplicate = 0;
            for (size_t h = base; h < s->note_count; h++) {
                if (s->notes[h].v.hidden_for_note_id == flick->v.id && s->notes[h].v.parent_note_id == begin->v.id) { duplicate = 1; break; }
            }
            if (duplicate) continue;
            note_t hidden = *flick;
            hidden.v.id = s->next_id++; hidden.v.operate_type = MS_OP_HIDDEN;
            hidden.v.parent_note_id = begin->v.id; hidden.v.hidden_for_note_id = flick->v.id;
            hidden.v.line_id = line; hidden.v.pair_note_id = -1; hidden.v.slide_along = 0;
            hidden.v.source_index = flick->v.source_index;
            if (!add_note(s, &hidden)) return 0;
        }
    }
    qsort(s->notes, s->note_count, sizeof(*s->notes), cmp_note); return 1;
}

static int add_combos(ms_score_t *s, uint32_t unit) {
    if (!unit) return 1;
    size_t base = s->note_count;
    /* Native walks the visible ViewNoteList.  Combo positions are aligned to
       the current bar/signature segment, not reset at each line segment. */
    for (size_t i = 0; i < base; i++) {
        if (s->notes[i].v.operate_type != MS_OP_SLIDE_BEGIN) continue;
        int line = s->notes[i].v.line_id;
        int parent = s->notes[i].v.id;
        size_t combo_index = 0;
        size_t members_cap = 8, members_n = 0;
        size_t *members = a_malloc(&s->alc, members_cap * sizeof(*members));
        if (!members) return 0;
        for (size_t j = 0; j < base; j++) {
            if (s->notes[j].v.line_id != line || !s->notes[j].v.visible) continue;
            if (members_n == members_cap) {
                members_cap *= 2;
                size_t *p = a_realloc(&s->alc, members, members_cap * sizeof(*members));
                if (!p) { a_free(&s->alc, members); return 0; }
                members = p;
            }
            members[members_n++] = j;
        }
        for (size_t m = 1; m < members_n; m++) {
            note_t *left = &s->notes[members[m - 1]], *right = &s->notes[members[m]];
            if (left->type[0] != 'l' || right->type[0] != 'l' || right->v.tick <= left->v.tick) continue;
            int32_t tick = left->v.tick;
            while (tick < right->v.tick) {
                ms_position_t pos;
                if (!position(s, tick, &pos)) break;
                double grid = (double)pos.rhythmic_unit / (double)unit;
                if (grid <= 0.0) break;
                double next = (floor((double)pos.rhythm / grid) + 1.0) * grid;
                int32_t step = (int32_t)ceil(next - (double)pos.rhythm - 1e-9);
                if (step < 1) step = 1;
                int32_t candidate = tick + step;
                if (candidate >= right->v.tick) break;
                tick = candidate;
                ms_position_t cp;
                if (!position(s, tick, &cp)) break;
                int conflict = 0;
                for (size_t q = 0; q < members_n; q++) {
                    note_t *other = &s->notes[members[q]];
                    if (other->v.id == parent) continue;
                    if (!judgement(other->v.operate_type)) continue;
                    int32_t delta = other->v.position.time_ms - cp.time_ms;
                    double bpm = 120.0;
                    for (size_t bi = 0; bi < s->bpm_count; bi++) {
                        if (s->bpms[bi].tick > tick) break;
                        bpm = s->bpms[bi].value;
                    }
                    if (delta > 0 && (double)delta <= 15000.0 / fmax(bpm, 1e-6)) { conflict = 1; break; }
                }
                for (size_t q = base; q < s->note_count && !conflict; q++) {
                    note_t *other = &s->notes[q];
                    if (!judgement(other->v.operate_type)) continue;
                    int32_t delta = other->v.position.time_ms - cp.time_ms;
                    if (delta > 0 && delta <= 125) conflict = 1;
                }
                note_t n; memset(&n, 0, sizeof(n));
                combo_index++;
                n.v.id = parent + (int32_t)(10000 * combo_index);
                n.v.operate_type = conflict ? MS_OP_COMBO_SKIP : MS_OP_COMBO;
                n.v.tick = tick; n.v.parent_note_id = parent; n.v.line_id = line;
                n.v.lane_count = LANES; n.v.visible = 1; n.v.slide_along = 1;
                n.v.source_index = left->v.source_index; n.v.position = cp;
                double ratio = (double)(tick - left->v.tick) / (double)(right->v.tick - left->v.tick);
                n.v.lane_start_float = left->v.lane_start_float + (right->v.lane_start_float - left->v.lane_start_float) * ratio;
                n.v.lane_end_float = left->v.lane_end_float + (right->v.lane_end_float - left->v.lane_end_float) * ratio;
                n.v.lane_start = (int32_t)floor(n.v.lane_start_float);
                n.v.lane_end = (int32_t)ceil(n.v.lane_end_float);
                n.v.width = n.v.lane_end_float - n.v.lane_start_float + 1.0;
                snprintf(n.type, sizeof(n.type), "node");
                if (!add_note(s, &n)) { a_free(&s->alc, members); return 0; }
            }
        }
        a_free(&s->alc, members);
    }
    qsort(s->notes, s->note_count, sizeof(*s->notes), cmp_note); return 1;
}

ms_result_t ms_score_parse(const void *data,size_t size,const ms_parse_options_t *options,const ms_allocator_t *allocator,ms_score_t **out,char *err,size_t errcap){
    if(!data||!size||!out){set_error(err,errcap,"data and output are required");return MS_ERR_INVALID_ARGUMENT;} const ms_allocator_t *a=allocator?allocator:&DEFAULT_ALC; if(!valid_alc(a)){set_error(err,errcap,"invalid allocator");return MS_ERR_INVALID_ARGUMENT;} *out=NULL; unsigned char *json=NULL;size_t json_n=0;ms_result_t r=inflate_input(a,data,size,&json,&json_n);if(r!=MS_OK){set_error(err,errcap,ms_result_string(r));return r;}
    ms_score_t *s=a_malloc(a,sizeof(*s));if(!s){a_free(a,json);return MS_ERR_OUT_OF_MEMORY;}memset(s,0,sizeof(*s));s->alc=*a;s->lane_count=LANES;s->next_id=options?options->start_note_id:0;s->mirror=options?options->mirror:0;
    yyjson_alc ya={yy_malloc,yy_realloc,yy_free,(void*)a}; yyjson_read_err je; yyjson_doc *doc=yyjson_read_opts((char*)json,json_n,0,&ya,&je);a_free(a,json);if(!doc){a_free(a,s);set_error(err,errcap,je.msg?je.msg:"json parse failed");return MS_ERR_JSON;}
    yyjson_val *root=yyjson_doc_get_root(doc);yyjson_val *score=obj(root,"score");if(!score)score=root;yyjson_val *events=obj(score,"events");yyjson_val *notes=obj(score,"notes");if(!yyjson_is_obj(score)||!events||!notes||!yyjson_is_arr(notes)){yyjson_doc_free(doc);ms_score_free(s);set_error(err,errcap,"missing or invalid score/events/notes");return MS_ERR_SCHEMA;} if(!build_events(s,events)){yyjson_doc_free(doc);ms_score_free(s);set_error(err,errcap,"invalid events");return MS_ERR_SCHEMA;}
    if(!parse_notes(s,notes,options?options->slide_combo_unit:0) ||
       (options && options->add_flick_hidden && !add_flick_hidden(s)) ||
       !add_combos(s,options?options->slide_combo_unit:0)){yyjson_doc_free(doc);ms_score_free(s);set_error(err,errcap,"note allocation failed");return MS_ERR_OUT_OF_MEMORY;} qsort(s->notes,s->note_count,sizeof(*s->notes),cmp_note);yyjson_doc_free(doc);*out=s;return MS_OK;
}

void ms_score_free(ms_score_t *s){if(!s)return;const ms_allocator_t*a=&s->alc;a_free(a,s->notes);a_free(a,s->bpms);a_free(a,s->sigs);a_free(a,s);}
size_t ms_score_note_count(const ms_score_t*s){return s?s->note_count:0;}
ms_result_t ms_score_note_at(const ms_score_t*s,size_t i,ms_note_view_t*out){if(!s||!out)return MS_ERR_INVALID_ARGUMENT;if(i>=s->note_count)return MS_ERR_RANGE;*out=s->notes[i].v;return MS_OK;}
int32_t ms_score_lane_count(const ms_score_t*s){return s?s->lane_count:0;}
size_t ms_score_bpm_count(const ms_score_t*s){return s?s->bpm_count:0;}
ms_result_t ms_score_bpm_at(const ms_score_t*s,size_t i,ms_bpm_event_t*out){if(!s||!out)return MS_ERR_INVALID_ARGUMENT;if(i>=s->bpm_count)return MS_ERR_RANGE;out->tick=s->bpms[i].tick;out->bpm=s->bpms[i].value;return position(s,s->bpms[i].tick,&out->position)?MS_OK:MS_ERR_INTERNAL;}
size_t ms_score_signature_count(const ms_score_t*s){return s?s->sig_count:0;}
ms_result_t ms_score_signature_at(const ms_score_t*s,size_t i,ms_signature_event_t*out){if(!s||!out)return MS_ERR_INVALID_ARGUMENT;if(i>=s->sig_count)return MS_ERR_RANGE;out->tick=s->sigs[i].tick;out->numerator=s->sigs[i].numerator;out->denominator=s->sigs[i].denominator;return position(s,s->sigs[i].tick,&out->position)?MS_OK:MS_ERR_INTERNAL;}
ms_result_t ms_score_position_at_tick(const ms_score_t*s,int32_t t,ms_position_t*out){return position(s,t,out)?MS_OK:MS_ERR_INVALID_ARGUMENT;}
size_t ms_score_line_member_count(const ms_score_t *s, int32_t line_id) { size_t n=0; if (!s) return 0; for (size_t i=0;i<s->note_count;i++) if (s->notes[i].v.line_id==line_id) n++; return n; }
ms_result_t ms_score_line_member_at(const ms_score_t *s, int32_t line_id, size_t index, ms_note_view_t *out) { if (!s||!out) return MS_ERR_INVALID_ARGUMENT; size_t n=0; for (size_t i=0;i<s->note_count;i++) if (s->notes[i].v.line_id==line_id) { if (n++==index) { *out=s->notes[i].v; return MS_OK; } } return MS_ERR_RANGE; }
uint8_t ms_operate_type_is_judgement(ms_operate_type_t t){return (uint8_t)(t!=MS_OP_NONE&&t!=MS_OP_HIDDEN_SLIDE_BEGIN&&t!=MS_OP_HIDDEN_SLIDE_END&&t!=MS_OP_GUIDE_BEGIN&&t!=MS_OP_GUIDE_END&&t!=MS_OP_COMBO_SKIP&&t!=MS_OP_HIDDEN&&t!=MS_OP_INVALID_HIDDEN);}
static int judgement(ms_operate_type_t t){return ms_operate_type_is_judgement(t);}
uint32_t ms_score_full_combo_count(const ms_score_t*s,uint8_t combos,uint8_t hidden){if(!s)return 0;uint32_t n=0;for(size_t i=0;i<s->note_count;i++){if(judgement(s->notes[i].v.operate_type)&& (combos|| (s->notes[i].v.operate_type!=MS_OP_COMBO&&s->notes[i].v.operate_type!=MS_OP_COMBO_SKIP)))n++;else if(hidden&&s->notes[i].v.hidden_for_note_id>=0)n++;}return n;}
size_t ms_score_command_count(const ms_score_t*s){return s?ms_score_full_combo_count(s,1,0):0;}
ms_result_t ms_score_command_at(const ms_score_t*s,size_t index,int32_t score_type,int32_t life,int32_t combo,ms_command_t*out){if(!s||!out)return MS_ERR_INVALID_ARGUMENT;size_t j=0;for(size_t i=0;i<s->note_count;i++)if(judgement(s->notes[i].v.operate_type)){if(j++==index){out->time_ms=s->notes[i].v.position.time_ms;out->current_combo=combo+(int32_t)index;out->current_life=life;out->note_id=s->notes[i].v.id;out->operate_type=s->notes[i].v.operate_type;out->score_type=score_type;return MS_OK;}}return MS_ERR_RANGE;}
