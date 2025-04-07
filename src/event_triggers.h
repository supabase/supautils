#ifndef EVENT_TRIGGERS_H
#define EVENT_TRIGGERS_H

typedef enum {
  FO_SEARCH_NAME,
  FO_SEARCH_FINFO
} func_owner_search_type;

typedef struct {
    func_owner_search_type as;
    union {
        List     *funcname;
        FmgrInfo *finfo;
    } val;
} func_owner_search;

extern Oid get_function_owner(func_owner_search search);

extern void force_noop(FmgrInfo *finfo);

extern bool is_event_trigger_function(Oid foid);

#endif
