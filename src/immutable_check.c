#include "pg_prelude.h"
#include "immutable_check.h"

static const char invalid_functions[][NAMEDATALEN] = {
    "now",
    "current_setting"
};
static const size_t num_invalid_funcs = sizeof(invalid_functions) / NAMEDATALEN;

static bool
contains_invalid_function_walker(Node *node, void *context)
{
    if (node == NULL)
        return false;

    if (IsA(node, FuncCall))
    {
        FuncCall *fcall = (FuncCall *) node;
        /*
         * fcall->funcname is a list of identifiers. If not schema-qualified,
         * it's just one element (the function name).
         * TODO: consider schema qualifications
         */
        if (fcall->funcname != NIL)
        {
            ListCell *fname = llast(fcall->funcname);
            if (fname && IsA(fname, String))
            {
                for(size_t i = 0; i < num_invalid_funcs; i++){
                    if (pg_strcasecmp(strVal(fname), invalid_functions[i]) == 0)
                        return true;
                }
            }
        }
    }

    // continue recursing into subnodes
    return raw_expression_tree_walker(node, contains_invalid_function_walker, context);
}

static bool
contains_invalid_function(Node *node, __attribute__((unused)) void *context)
{
    return raw_expression_tree_walker(node, contains_invalid_function_walker, NULL);
}

bool contains_non_immutable_check_constraints(CreateStmt *stmt){
    ListCell   *lc;
    /*
     *Look for tableElts which can contain ColumnDefs and TableConstraints
     *TODO: also consider TableConstraints
     */
    foreach(lc, stmt->tableElts)
    {
      Node *elt = (Node *) lfirst(lc);

      if (elt == NULL)
          continue;

      // If it's a ColumnDef, check `constraints` field.
      if (IsA(elt, ColumnDef))
      {
          ColumnDef  *cdef = (ColumnDef *) elt;
          ListCell   *constraint_lc;

          foreach(constraint_lc, cdef->constraints)
          {
              Constraint *constr = (Constraint *) lfirst(constraint_lc);
              if (constr->contype == CONSTR_CHECK && constr->raw_expr)
              {
                  if (contains_invalid_function((Node *) constr->raw_expr, NULL))
                      return true;
              }
          }
      }
    }

    return false;
}

