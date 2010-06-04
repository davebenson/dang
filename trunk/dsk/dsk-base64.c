
#define BASE64_LINE_LENGTH  64    /* TODO: look this up!!!!!! */

/* Map from 0..63 as characters. */
char dsk_base64_value_to_char[64] = {
#include "dsk-base64-char-table.inc"
};


/* Map from character to 0..63
 * or -1 for bad byte; -2 for whitespace; -3 for equals */
int8_t dsk_base64_char_to_value[256] = {
#include "dsk-base64-value-table.inc"
};

/* --- encoder --- */
struct _DskBase64EncoderClass
{
  DskOctetFilterClass base_class;
};
struct _DskBase64Encoder
{
  DskOctetFilter base_instance;

  /* if -1, then don't break lines; in output bytes; never 0 */
  int length_remaining;
};

#define dsk_base64_encoder_init NULL
#define dsk_base64_encoder_finalize NULL


static dsk_boolean
dsk_base64_encoder_process (DskOctetFilter *filter,
                            DskBuffer      *out,
                            unsigned        in_length,
                            const uint8_t  *in_data,
                            DskError      **error)
{
  ...
}
static dsk_boolean
dsk_base64_encoder_finish(DskOctetFilter *filter,
                          DskBuffer      *out,
                          DskError      **error)
{
  ...
}

DSK_OCTET_FILTER_SUBCLASS_DEFINE(static, DskBase64Encoder, dsk_base64_encoder);


DskOctetFilter *dsk_base64_encoder_new             (dsk_boolean break_lines)
{
  DskBase64Filter *rv = dsk_object_new (&dsk_base64_encoder_class);
  rv->length_remaining = break_line ? BASE64_LINE_LENGTH : -1;
  return DSK_OCTET_FILTER (rv);
}
DskOctetFilter *dsk_base64_decoder_new             (void)
{
  ...
}
