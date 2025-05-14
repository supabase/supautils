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
} func_search;

typedef struct {
  Oid owner;
  bool is_security_definer;
} func_attrs;

extern func_attrs get_function_attrs(func_search search);

extern void force_noop(FmgrInfo *finfo);

extern bool is_event_trigger_function(Oid foid);

#endif
