#include <string.h>
#include "dsk.h"

DskXml *dsk_xml_ref   (DskXml *xml)
{
  ++(xml->ref_count);
  return xml;
}

static inline void
dsk_xml_filename_unref (DskXmlFilename *filename)
{
  if (--(filename->ref_count) == 0)
    dsk_free (filename);
}

void    dsk_xml_unref (DskXml *xml)
{
restart:
  if (--(xml->ref_count) == 0)
    {
      if (xml->filename)
        dsk_xml_filename_unref (xml->filename);
      if (xml->type == DSK_XML_ELEMENT)
        {
          unsigned n = xml->n_children;
          DskXml *tail = NULL;
          unsigned i;
          for (i = 0; i < n; i++)
            {
              if (xml->children[i]->ref_count > 1)
                --(xml->children[i]->ref_count);
              else if (tail)
                dsk_xml_unref (xml->children[i]);
              else
                tail = xml->children[i];
            }
          if (tail)
            {
              dsk_free (xml);
              xml = tail;
              goto restart;
            }
        }
      dsk_free (xml);
    }
}

/* --- TODO: add wad of constructors --- */
DskXml *dsk_xml_text_new_len (unsigned len,
                              const char *data)
{
  DskXml *xml = dsk_malloc (len + 1 + sizeof (DskXml));
  memcpy (xml + 1, data, len);
  xml->str = (char*) xml + 1;
  xml->str[len] = 0;
  xml->filename = NULL;
  xml->line_no = 0;
  xml->type = DSK_XML_TEXT;
  xml->ref_count = 1;
  return xml;
}

DskXml *dsk_xml_comment_new_len (unsigned len,
                                 const char *text)
{
  DskXml *rv = dsk_xml_comment_new_len (len, text);
  rv->type = DSK_XML_COMMENT;
  return rv;
}

DskXml *_dsk_xml_new_elt_parse (unsigned n_attrs,
                                unsigned name_kv_space,
                                const char *name_and_attrs,
                                unsigned n_children,
                                DskXml **children)
{
  DskXml *rv = dsk_malloc (sizeof (char *) * (n_attrs * 2 + 1)
                           + sizeof (DskXml *) * n_children
                           + name_kv_space
                           + sizeof (DskXml));
  unsigned i;
  char *at;
  rv->line_no = 0;
  rv->filename = NULL;
  rv->type = DSK_XML_ELEMENT;
  rv->ref_count = 1;
  rv->n_children = n_children;
  rv->attrs = (char **) (rv + 1);
  rv->children = (DskXml **) (rv->attrs + n_attrs * 2 + 1);
  rv->str = (char*) (rv->children + n_children);
  memcpy (rv->str, name_and_attrs, name_kv_space);
  at = strchr (rv->str, 0) + 1;
  for (i = 0; i < 2*n_attrs; i++)
    {
      rv->attrs[i] = at;
      at = strchr (at, 0) + 1;
    }
  memcpy (rv->children, children, sizeof(DskXml *) * n_children);
  for (i = 0; i < n_children; i++)
    children[i]->ref_count += 1;
  return rv;
}

void _dsk_xml_set_position (DskXml *xml,
                            DskXmlFilename *filename,
                            unsigned line_no)
{
  dsk_assert (xml->filename == NULL);
  xml->filename = filename;
  filename->ref_count++;
  xml->line_no = line_no;
}
