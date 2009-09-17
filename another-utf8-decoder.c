/* from gtkmm */
/* http://git.gnome.org/cgit/glibmm/tree/glib/glibmm/ustring.cc#n270 */
uint32_t get_unichar_from_std_iterator(const char *at)
{
  unsigned int result = * (const unsigned char *) at;

  if((result & 0x80) != 0)
    {
      unsigned int mask = 0x40;

      do
        {
          unsigned c;
          result <<= 6;
          at++;
          c = * (unsigned char) (*at);
          mask <<= 5;
          result += c - 0x80;
        }
      while((result & mask) != 0);

      result &= mask - 1;
    }

  return result;
}
