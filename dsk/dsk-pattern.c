#include <string.h>
#include "dsk.h"
#include "../gsklistmacros.h"           /* TODO: USE THESE */

#include "dsk-pattern.h"  /* TEMP INCLUDE */

struct CharClass
{
  uint8_t set[32];
};
#define MK_LITERAL_CHAR_CLASS(c) ((struct CharClass *) (size_t) (uint8_t) (c))
#define IS_SINGLE_CHAR_CLASS(cl) (((size_t)(cl)) < 256)
#define CHAR_CLASS_SET(cl, v)    cl->set[((uint8_t)(v))/8] |= (1 << ((uint8_t)(v) % 8))
#define SINGLE_CHAR_CLASS_GET_CHAR(cl)     ((uint8_t)(size_t)(cl))

static void
char_class_union_inplace (struct CharClass *inout, const struct CharClass *addend)
{
  unsigned i;
  if (IS_SINGLE_CHAR_CLASS (addend))
    CHAR_CLASS_SET (inout, SINGLE_CHAR_CLASS_GET_CHAR (addend));
  else
    for (i = 0; i < DSK_N_ELEMENTS (inout->set); i++)
      inout->set[i] |= addend->set[i];
}

#define COMPARE_INT(a,b)   ((a)<(b) ? -1 : (a)>(b) ? 1 : 0)

static int
compare_char_class_to_single (const struct CharClass *a,
                              uint8_t                 b)
{
  const uint8_t *A = a->set;
  unsigned i;
  unsigned v;
  for (i = 0; i < b/8; i++)
    if (A[i] > 0)
      return 1;
  v = 1 << (b%8);
  if (A[i] < v)
    return -1;
  if (A[i] > v)
    return +1;
  for (i++; i < 32; i++)
    if (A[i] > 0)
      return 1;
  return 0;
}

static int
compare_char_classes (const struct CharClass *a,
                      const struct CharClass *b)
{
  if (IS_SINGLE_CHAR_CLASS (a))
    {
      if (IS_SINGLE_CHAR_CLASS (b))
        return COMPARE_INT (SINGLE_CHAR_CLASS_GET_CHAR (a),
                            SINGLE_CHAR_CLASS_GET_CHAR (b));
      else
        return -compare_char_class_to_single (b, SINGLE_CHAR_CLASS_GET_CHAR (a));
    }
  else
    {
      if (IS_SINGLE_CHAR_CLASS (b))
        return compare_char_class_to_single (b, SINGLE_CHAR_CLASS_GET_CHAR (a));
      else
        return memcmp (a->set, b->set, sizeof (struct CharClass));
    }
}

/* --- Pattern -- A Parsed Regex --- */
typedef enum
{
  PATTERN_LITERAL,
  PATTERN_CONCAT,               /* concatenation */
  PATTERN_ALT,                  /* alternation */
  PATTERN_OPTIONAL,             /* optional pattern */
  PATTERN_STAR,                 /* repeated 0 or more times */
  PATTERN_PLUS,                 /* repeated 1 or more times */
  PATTERN_EMPTY
} PatternType;

struct Pattern
{
  PatternType type;
  union
  {
    struct CharClass *literal;
    struct { struct Pattern *a, *b; } concat;
    struct { struct Pattern *a, *b; } alternation;
    struct Pattern *optional;
    struct Pattern *star;
    struct Pattern *plus;
  } info;
};

typedef enum
{
  TOKEN_LPAREN,
  TOKEN_RPAREN,
  TOKEN_ALTER,
  TOKEN_STAR,
  TOKEN_PLUS,
  TOKEN_QUESTION_MARK,
  TOKEN_PATTERN         /* whilst parsing */
} TokenType;

struct Token
{
  TokenType type;
  struct Pattern *pattern;
  struct Token *prev, *next;
};

static struct {
  char c;
  struct CharClass cclass;
} special_char_classes[] = {
#include "dsk-pattern-char-classes.inc"
};

/* Parse a backslash-escape special character class,
   include a lot of punctuation.  The backslash
   should already have been skipped. */
static dsk_boolean
get_backslash_char_class (const char **p_at, struct CharClass **out)
{
  const char *start = *p_at;
  char bs = *start;
  if (dsk_ascii_ispunct (bs))
    *out = MK_LITERAL_CHAR_CLASS (bs);
  else if (bs == 'n')
    *out = MK_LITERAL_CHAR_CLASS ('\n');
  else if (bs == 'r')
    *out = MK_LITERAL_CHAR_CLASS ('\r');
  else if (bs == 't')
    *out = MK_LITERAL_CHAR_CLASS ('\t');
  else if (bs == 'v')
    *out = MK_LITERAL_CHAR_CLASS ('\v');
  else if (dsk_ascii_isdigit (bs))
    {
      uint8_t value;
      if (!dsk_ascii_isdigit (start[1]))
        {
          value = start[0] - '0';
          *p_at = start + 1;
        }
      else if (!dsk_ascii_isdigit (start[2]))
        {
          value = (start[0] - '0') * 8 + (start[1] - '0');
          *p_at = start + 2;
        }
      else
        {
          value = (start[0] - '0') * 64 + (start[1] - '0') * 8 + (start[2] - '0');
          *p_at = start + 3;
        }
      *out = MK_LITERAL_CHAR_CLASS (value);
      return DSK_TRUE;
    }
  else
    {
      unsigned i;
      for (i = 0; i < DSK_N_ELEMENTS (special_char_classes); i++)
        if (special_char_classes[i].c == bs)
          {
            *out = &special_char_classes[i].cclass;
            *p_at = start + 1;
            return DSK_TRUE;
          }
      return DSK_FALSE;
    }
  *p_at = start + 1;
  return DSK_TRUE;
}

/* Parse a [] character class expression */
static struct CharClass *
parse_character_class (const char **p_regex,
                       DskError   **error)
{
  const char *at = *p_regex;
  dsk_boolean reverse = DSK_FALSE;
  struct CharClass *out = dsk_malloc0 (sizeof (struct CharClass));
  if (*at == '^')
    {
      reverse = DSK_TRUE;
      at++;
    }
  while (*at != 0 && *at != ']')
    {
      /* this muck is structured annoyingly:  we just to the label
         got_range_start_and_dash whenever we encounter a '-' after
         a single character (either literally or as a backslash sequence),
         to handle range expressions. */
      unsigned first_value;

      if (*at == '\\')
        {
          struct CharClass *sub;
          at++;
          if (!get_backslash_char_class (&at, &sub))
            {
              *p_regex = at;    /* for error reporting (maybe?) */
              dsk_set_error (error, "bad \\ expression (at %s)", dsk_ascii_byte_name (*at));
              return NULL;
            }
          if (IS_SINGLE_CHAR_CLASS (sub) && *at == '-')
            {
              first_value = SINGLE_CHAR_CLASS_GET_CHAR (sub);
              at++;
              goto got_range_start_and_dash;
            }
          char_class_union_inplace (out, sub);
        }
      else if (at[1] == '-')
        {
          first_value = *at;
          at += 2;
          goto got_range_start_and_dash;
        }
      else
        {
          /* single character */
          CHAR_CLASS_SET (out, *at);
          at++;
        }

      continue;
got_range_start_and_dash:
      {
        unsigned last_value;
        unsigned code;
        if (*at == '\\')
          {
            struct CharClass *sub;
            const char *start;
            at++;
            start = at;
            if (!get_backslash_char_class (&at, &sub))
              {
                *p_regex = at;    /* for error reporting (maybe?) */
                dsk_set_error (error, "bad \\ expression (at %s)", dsk_ascii_byte_name (*at));
                return NULL;
              }
            if (!IS_SINGLE_CHAR_CLASS (sub))
              {
                dsk_set_error (error, "non-single-byte \\%c encountered - cannot use in range", *start);
                return NULL;
              }
            last_value = SINGLE_CHAR_CLASS_GET_CHAR (sub);
          }
        else if (*at == ']')
          {
            /* syntax error */
            dsk_set_error (error, "unterminated character class range");
            return NULL;
          }
        else
          {
            last_value = *at;
            at++;
          }

        if (first_value > last_value)
          {
            dsk_set_error (error, "character range is not first<last (first=%s, last=%s)",
                           dsk_ascii_byte_name (first_value),
                           dsk_ascii_byte_name (last_value));
            return NULL;
          }
        for (code = first_value; code <= last_value; code++)
          CHAR_CLASS_SET (out, code);
      }
    }
  *p_regex = at;
  return out;
}

static dsk_boolean
tokenize (const char   *regex,
          struct Token **token_list_out,
          DskMemPool   *pool,
          DskError    **error)
{
  struct Token *last = NULL;
  *token_list_out = NULL;
  while (*regex)
    {
      struct Token *t = dsk_mem_pool_alloc (pool, sizeof (struct Token));
      switch (*regex)
        {
        case '*':
          t->type = TOKEN_STAR;
          regex++;
          break;
        case '+':
          t->type = TOKEN_PLUS;
          regex++;
          break;
        case '?':
          t->type = TOKEN_QUESTION_MARK;
          regex++;
          break;
        case '(':
          t->type = TOKEN_LPAREN;
          regex++;
          break;
        case ')':
          t->type = TOKEN_RPAREN;
          regex++;
          break;
        case '|':
          t->type = TOKEN_ALTER;
          regex++;
          break;
        case '[':
          {
            struct CharClass *cclass;
            /* parse character class */
            regex++;
            cclass = parse_character_class (&regex, error);
            if (cclass == NULL || *regex != ']')
              return DSK_FALSE;
            regex++;
            t->type = TOKEN_PATTERN;
            t->pattern = dsk_mem_pool_alloc (pool, sizeof (struct Pattern));
            t->pattern->type = PATTERN_LITERAL;
            t->pattern->info.literal = cclass;
            break;
          }
        case '\\':
          {
            /* parse either char class or special literal */
            struct CharClass *cclass;
            regex++;
            if (get_backslash_char_class (&regex, &cclass))
              {
                t->type = TOKEN_PATTERN;
                t->pattern = dsk_mem_pool_alloc (pool, sizeof (struct Pattern));
                t->pattern->type = PATTERN_LITERAL;
                t->pattern->info.literal = cclass;
              }
            else
              {
                if (regex[1] == 0)
                  dsk_set_error (error, "unexpected backslash sequence in regex");
                else
                  dsk_set_error (error, "bad char %s after backslash", dsk_ascii_byte_name (regex[1]));
                return DSK_FALSE;
              }
            break;
          }
        default:
          /* character literal */
          t->type = TOKEN_PATTERN;
          t->pattern = dsk_mem_pool_alloc (pool, sizeof (struct Pattern));
          t->pattern->type = PATTERN_LITERAL;
          t->pattern->info.literal = MK_LITERAL_CHAR_CLASS (regex[0]);
          regex++;
          break;
        }

      /* append to list */
      t->prev = last;
      t->next = NULL;
      if (last)
        last->next = t;
      else
        *token_list_out = last = t;
    }
  return DSK_TRUE;
}

static struct Pattern *
parse_pattern (unsigned      pattern_index,
               struct Token *token_list,
               DskMemPool   *pool,
               DskError    **error)
{
  /* Handle parens */
  struct Token *token;
  for (token = token_list; token; token = token->next)
    if (token->type == TOKEN_LPAREN)
      {
        /* find matching rparen (or error) */
        struct Token *rparen = token->next;
        int balance = 1;
        while (rparen)
          {
            if (rparen->type == TOKEN_LPAREN)
              balance++;
            else if (rparen->type == TOKEN_RPAREN)
              {
                balance--;
                if (balance == 0)
                  break;
              }
          }
        if (balance)
          {
            /* missing right-paren */
            dsk_set_error (error, "missing right-paren in regex");
            return NULL;
          }

        /* recurse */
        rparen->prev->next = NULL;
        struct Pattern *subpattern;
        subpattern = parse_pattern (pattern_index, token->next, pool, error);
        if (subpattern == NULL)
          return NULL;

        /* replace parenthesized expr with subpattern; slice out remainder of list */
        token->type = TOKEN_PATTERN;
        token->pattern = subpattern;
        token->next = rparen->next;
        if (rparen->next)
          token->next->prev = token;
      }
    else if (token->type == TOKEN_RPAREN)
      {
        dsk_set_error (error, "unexpected right-paren in regex");
        return NULL;
      }

  /* Handle star/plus/qm */
  for (token = token_list; token; token = token->next)
    if (token->type == TOKEN_QUESTION_MARK
     || token->type == TOKEN_STAR
     || token->type == TOKEN_PLUS)
      {
        if (token->prev == NULL || token->prev->type != TOKEN_PATTERN)
          {
            dsk_set_error (error, "'%c' must be precede by pattern",
                           token->type == TOKEN_QUESTION_MARK ? '?' 
                           : token->type == TOKEN_STAR ? '*'
                           : '+');
            return NULL;
          }
        struct Pattern *new_pattern = dsk_mem_pool_alloc (pool, sizeof (struct Pattern));
        switch (token->type)
          {
          case TOKEN_QUESTION_MARK:
            new_pattern->type = PATTERN_OPTIONAL;
            new_pattern->info.optional = token->prev->pattern;
            break;
          case TOKEN_STAR:
            new_pattern->type = PATTERN_STAR;
            new_pattern->info.star = token->prev->pattern;
            break;
          case TOKEN_PLUS:
            new_pattern->type = PATTERN_PLUS;
            new_pattern->info.plus = token->prev->pattern;
            break;
          default:
            dsk_assert_not_reached ();
          }
        token->prev->pattern = new_pattern;

        /* remove token */
        if (token->prev)
          token->prev->next = token->next;
        else
          token_list = token->next;
        if (token->next)
          token->next->prev = token->prev;
        /* token isn't in the list now!  but it doesn't matter b/c token->next is still correct */
      }

  /* Handle concatenation */
  for (token = token_list; token && token->next; token = token->next)
    {
      if (token->type == TOKEN_PATTERN
       && token->next->type == TOKEN_PATTERN)
        {
          /* concat */
          struct Pattern *new_pattern = dsk_mem_pool_alloc (pool, sizeof (struct Pattern));
          new_pattern->type = PATTERN_CONCAT;
          new_pattern->info.concat.a = token->pattern;
          new_pattern->info.concat.b = token->next->pattern;
          token->pattern = new_pattern;

          /* remove token->next */
          struct Token *kill = token->next;
          token->next = kill->next;
          if (kill->next)
            kill->next->prev = token;
        }
    }

  /* At this point we consist of nothing but alternations and
     patterns.  Scan through, discarding TOKEN_ALTER,
     and keeping track of whether the empty pattern matches */
  dsk_boolean last_was_alter = DSK_TRUE;/* trick the empty pattern detector
                                           into triggering on initial '|' */
  dsk_boolean accept_empty = DSK_FALSE;
  for (token = token_list; token; token = token->next)
    if (token->type == TOKEN_ALTER)
      {
        if (last_was_alter)
          accept_empty = DSK_TRUE;
        last_was_alter = DSK_TRUE;

        /* remove token from list */
        if (token->prev)
          token->prev->next = token->next;
        else
          token_list = token->next;
        if (token->next)
          token->next->prev = token->prev;
      }
    else
      {
        last_was_alter = DSK_FALSE;
      }
  if (last_was_alter)
    accept_empty = DSK_TRUE;

  /* if we accept an empty token, toss a PATTERN_EMPTY onto the list of patterns
     in the alternation. */
  if (accept_empty || token_list == NULL)
    {
      struct Token *t = dsk_mem_pool_alloc (pool, sizeof (struct Token));
      t->next = token_list;
      t->prev = NULL;
      if (t->next)
        t->next->prev = t;
      token_list = t;

      t->type = TOKEN_PATTERN;
      t->pattern = dsk_mem_pool_alloc (pool, sizeof (struct Pattern));
    }

  /* At this point, token_list!=NULL,
     and it consists entirely of patterns.
     Reduce it to a singleton with the alternation pattern. */
  while (token_list->next != NULL)
    {
      /* create alternation pattern */
      struct Pattern *new_pattern = dsk_mem_pool_alloc (pool, sizeof (struct Pattern));
      new_pattern->type = PATTERN_ALT;
      new_pattern->info.alternation.a = token->pattern;
      new_pattern->info.alternation.b = token->next->pattern;
      token->pattern = new_pattern;

      /* remove token->next */
      struct Token *kill = token->next;
      token->next = kill->next;
      if (kill->next)
        kill->next->prev = token;
    }

  /* Return value consists of merely a single token-list. */
  dsk_assert (token_list != NULL && token_list->next == NULL);
  return token_list->pattern;
}

/* --- Nondeterministic Finite Automata --- */
struct NFA_Transition
{
  struct CharClass *char_class;               /* if NULL then no char consumed */
  struct NFA_State *next_state;
  struct NFA_Transition *next_in_state;
};

struct NFA_State
{
  struct Pattern *pattern;
  struct NFA_Transition *transitions;
  unsigned is_match : 1;
  unsigned has_been_denullified : 1;
  unsigned pattern_index : 30;
};

#define NFA_STATE_TRANSITION_STACK(head) \
   struct NFA_Transition *, (head), next_in_state
#define NFA_STATE_GET_TRANSITION_STACK(state) \
   NFA_STATE_TRANSITION_STACK((state)->transitions)

static inline void
prepend_transition (struct NFA_Transition **plist,
                    struct CharClass       *char_class,
                    struct NFA_State       *next_state,
                    DskMemPool             *pool)
{
  struct NFA_Transition *t = dsk_mem_pool_alloc (pool, sizeof (struct NFA_Transition));
  t->char_class = char_class;
  t->next_state = next_state;
  t->next_in_state = *plist;
  *plist = t;
}

/* Converting pattern to NFS_State, allowing transitions that
   don't consume characters */
static struct NFA_State *
pattern_to_nfa_state (struct Pattern *pattern,
                      unsigned pattern_index,
                      struct NFA_State *result,
                      DskMemPool *pool)
{
  struct NFA_State *b, *rv;
  struct NFA_Transition *trans;
tail_recurse:
  switch (pattern->type)
    {
    case PATTERN_LITERAL:
      /* Allocate state with one transition */
      rv = dsk_mem_pool_alloc (pool, sizeof (struct NFA_State));
      rv->pattern_index = pattern_index;
      rv->pattern = pattern;
      rv->transitions = NULL;
      prepend_transition (&rv->transitions, pattern->info.literal, result, pool);
      break;
      
    case PATTERN_ALT:
      rv = pattern_to_nfa_state (pattern->info.alternation.a, pattern_index, result, pool);
      b = pattern_to_nfa_state (pattern->info.alternation.b, pattern_index, result, pool);
      for (trans = b->transitions; trans; trans = trans->next_in_state)
        prepend_transition (&rv->transitions, trans->char_class, trans->next_state, pool);
      break;
    case PATTERN_CONCAT:
      /* NOTE: we handle tail_recursion to benefit long strings of concatenation
             (((('a' . 'b') . 'c') . 'd') . 'e')
         thus, the parser should be careful to arrange the concat patterns thusly */
      b = pattern_to_nfa_state (pattern->info.concat.b, pattern_index, result, pool);
      pattern = pattern->info.concat.a;
      result = b;
      goto tail_recurse;
    case PATTERN_OPTIONAL:
      rv = pattern_to_nfa_state (pattern->info.optional, pattern_index, result, pool);
      prepend_transition (&rv->transitions, NULL, result, pool);
      break;
    case PATTERN_PLUS:
      rv = pattern_to_nfa_state (pattern->info.plus, pattern_index, result, pool);
      prepend_transition (&result->transitions, NULL, rv, pool);
      break;
    case PATTERN_STAR:
      rv = pattern_to_nfa_state (pattern->info.plus, pattern_index, result, pool);
      prepend_transition (&rv->transitions, NULL, result, pool);
      prepend_transition (&result->transitions, NULL, rv, pool);
      break;
    default:
      dsk_assert_not_reached ();
    }
  return rv;
}

static int
compare_transitions_full (const struct NFA_Transition *a,
                          const struct NFA_Transition *b)
{
  int rv = compare_char_classes (a->char_class, b->char_class);
  if (rv != 0)
    return rv;
  return a->next_state < b->next_state ? -1
       : a->next_state > b->next_state ? +1
       : 0;
}

static void
uniq_transitions (struct NFA_State *state)
{
  /* sort transitions */
#define COMPARE_TRANSITIONS(a,b, rv) rv = compare_transitions_full(a,b)
  GSK_STACK_SORT (NFA_STATE_GET_TRANSITION_STACK (state), COMPARE_TRANSITIONS);
#undef COMPARE_TRANSITIONS

  /* remove dups */
  struct NFA_Transition *trans;
  for (trans = state->transitions; trans != NULL; )
    {
      if (compare_transitions_full (trans, trans->next_in_state) == 0)
        {
          /* remove next */
          ...
        }
      else
        trans = trans->next_in_state;
    }
}

/* Remove all transitions with char_class==NULL
   by copying the target's transitions into the current state's list.
   Then recurse. */
static void
nfa_prune_null_transitions (struct NFA_State *state,
                            DskMemPool       *pool)
{
  /* Split the transition list into two halves -- one that contains 
     CharClass==NULL, and the other that doesn't */
  struct NFA_Transition *null_transitions;
  struct NFA_Transition *unhandled_null_transitions;
  struct NFA_Transition **ptrans;
restart:
  null_transitions = NULL;
  unhandled_null_transitions = NULL;
  for (ptrans = &state->transitions; *ptrans != NULL; )
    if ((*ptrans)->char_class == NULL)
      {
        struct NFA_Transition *to_reduce = *ptrans;
        *ptrans = to_reduce->next_in_state;
        to_reduce->next_in_state = unhandled_null_transitions;
        unhandled_null_transitions = to_reduce;
      }
    else
      {
        ptrans = &((*ptrans)->next_in_state);
      }

  /* For each CharClass==NULL transition, add its real transitions 
     our pruned list (dedup) and its NULL transitions to our "to-process" list.
     Use a marker to avoid cycles. */
  while (unhandled_null_transitions)
    {
      struct NFA_Transition *t = *ptrans;
      for (t = null_transitions; t; t = t->next_in_state)
        if (t->next_state == unhandled_null_transitions->next_state)
          break;
      if (t)
        unhandled_null_transitions = unhandled_null_transitions->next_in_state;
      else
        {
          struct NFA_Transition *p = unhandled_null_transitions;
          struct NFA_State *dest = p->next_state;
          unhandled_null_transitions = p->next_in_state;
          p->next_in_state = null_transitions;
          null_transitions = p;

          if (dest->is_match)
            {
              state->is_match = 1;

          /* Copy all ParseState's from dest to
             either unhandled_null_transitions or state's transition list */
          for (p = dest->transitions; p; p = p->next_in_state)
            prepend_transition (p->char_class != NULL
                                    ? &state->transitions
                                    : &unhandled_null_transitions,
                                p->char_class, p->next_state,
                                pool);
        }
    }

  uniq_transitions (state);

  state->has_been_denullified = 1;

  /* Recurse / tail-recurse */
  struct NFA_State *tail_recurse = NULL;
  struct NFA_Transition *trans;
  for (trans = state->transitions; trans; trans = trans->next_in_state)
    if (!trans->next_state->has_been_denullified)
      {
        if (tail_recurse == NULL)
          tail_recurse = trans->next_state;
        else
          nfa_prune_null_transitions (trans->next_state, pool);
      }
  if (tail_recurse)
    {
      state = tail_recurse;
      goto restart;
    }
}

#if 0
/* --- Deterministic Finite Automata --- */
/* Convert sets of states into single states with transitions 
   mutually exclusive */

struct _Transition
{
  char c;
  State *next;
};

struct _State
{
  unsigned hash;
  unsigned n_transitions;
  Transition *transitions;
  State *default_next;
  State *hash_next;
  State *uninitialized_next;
  unsigned positions[1];
};

struct _StateSlab
{
  StateSlab *next_slab;
};

struct _StateTable
{
  unsigned n_entries;
  unsigned table_size;
  State **hash_table;
  unsigned occupancy;

  unsigned sizeof_slab;
  unsigned sizeof_state;
  unsigned n_states_in_slab;
  unsigned n_states_free_in_slab;
  State *next_free_in_cur_slab;
  StateSlab *cur_slab;

  State *uninitialized_list;
};

typedef enum
{
  REPEAT_EXACT,
  REPEAT_OPTIONAL,
  REPEAT_STAR,
  REPEAT_PLUS
} RepeatType;

struct _ParsedEntry
{
  CharClass *char_class;
  RepeatType repeat_type;
};

/* things are parsed in order, each entry adds N states:
     EXACT:    1 -- is after char
     OPTIONAL: 1 -- is after char
     REPEATED: ...
 */

static void
state_table_init (StateTable *table, unsigned n_entries)
{
  table->n_entries = n_entries;
  table->table_size = 19;
  table->hash_table = dsk_malloc0 (sizeof (State*) * table->table_size);
  table->occupancy = 0;
  table->sizeof_state = DSK_ALIGN (sizeof (State) + (n_entries-1) * sizeof(unsigned), sizeof (void*));
  table->n_states_in_slab = 16;
  table->sizeof_slab = sizeof (StateSlab) + table->sizeof_state * table->n_states_in_slab;
  table->n_states_free_in_slab = 0;
  table->next_free_in_cur_slab = NULL;		/* unneeded */
  table->cur_slab = NULL;
  table->uninitialized_list = NULL;
};

static State *
state_table_force (StateTable *table, const unsigned *positions)
{
  unsigned hash = state_hash (table->n_entries, positions);
  unsigned idx = hash % table->n_entries;
  State *at = table->hash_table[idx];
  while (at != NULL)
    {
      if (memcmp (at->positions, positions, sizeof (unsigned) * table->n_entries) == 0)
        return at;
      at = at->hash_next;
    }

  /* Allocate a state */
  if (table->n_states_free_in_slab == 0)
    {
      StateSlab *slab = dsk_malloc (table->sizeof_slab);
      table->next_free_in_cur_slab = (State *)(slab + 1);
      table->n_states_free_in_slab = table->n_states_in_slab;
      slab->next_slab = table->cur_slab;
      table->cur_slab = slab;
    }
  at = table->next_free_in_cur_slab;
  table->next_free_in_cur_slab = (State *) ((char*)table->next_free_in_cur_slab + table->sizeof_state);
  table->n_states_free_in_slab -= 1;

  table->occupancy++;
  if (table->occupancy * 2 > table->table_size)
    {
      unsigned new_table_size = ...;
      ...
      idx = hash % table->table_size;
    }

  /* Initialize it */
  memcpy (at->positions, positions, sizeof (unsigned) * table->n_entries);
  at->hash = hash;
  at->n_transitions = 0;
  at->transitions = NULL;
  at->default_next = NULL;

  /* add to hash-table */
  at->hash_next = table->hash_table[idx];
  table->hash_table[idx] = at;

  /* add to uninitialized list */
  at->uninitialized_next = table->uninitialized_list;
  table->uninitialized_list = at;

  return at;
}
#endif

DskPattern *dsk_pattern_compile (unsigned n_entries,
                                 DskPatternEntry *entries)
{
  ...
}
