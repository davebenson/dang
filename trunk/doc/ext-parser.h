
ExtParser *ext_parser_new (void);

typedef struct {
  unsigned token_id;
  union { int v_int;
          unsigned v_uint;
	  int64_t v_int64;
	  uint64_t v_uint64;
	  float v_float;
	  double v_double;
          char *v_string;
	  void *v_pointer; } data[4];
  const char *filename;
  unsigned line;
} ExtParserData;

typedef char *(*ExtParserToString) (ExtParserData *data);
typedef void  (*ExtParserDestruct) (ExtParserData *data);

typedef enum
{
  EXT_PARSER_RESULT_SUCCESS,    /* must be 0 */
  EXT_PARSER_RESULT_IGNORE,     /* non-fatal error */
  EXT_PARSER_RESULT_ERROR       /* see output->data[0].v_string for msg */
} ExtParserResult;
typedef ExtParserResult  (*ExtParserFunc)  (unsigned          n_inputs,
                                            ExtParserData   **inputs,
                                            ExtParserData    *output,
                                            void             *data);

/* defining the parser */
void       ext_parser_register (ExtParser *parser,
                                unsigned   token_id,
				const char *name,
				ExtParserToString to_string,
				ExtParserDestruct destruct);
void       ext_parser_define_rule (ExtParser *parser,
                                   unsigned   n_inputs,
				   unsigned  *input_token_ids,
				   unsigned   output_token_id,
				   ExtParserFunc func,
				   void         *data,
				   ExtParserDestroyFunc destroy);


/* parsing */
void            ext_parser_push     (ExtParser *parser,
                                     ExtParserData *input);
ExtParserResult ext_parser_pop      (ExtParser *parser,
                                     unsigned   token_id,
				     ExtParserData *data);
