struct _DskTableCheckpointIface
{
  DskTableCheckpoint *(*create)  (DskTableCheckpointIface *iface,
                                  int                 openat_fd,
                                  const char         *basename,
                                  unsigned            cp_data_len,
                                  const uint8_t      *cp_data,
                                  DskError          **error);
  DskTableCheckpoint *(*open)    (int                 openat_fd,
                                  const char         *basename,
                                  unsigned           *cp_data_len_out,
                                  uint8_t           **cp_data_out,
                                  DskTableCheckpointReplayFunc func,
                                  void               *func_data,
                                  DskError          **error);
};

struct _DskTableCheckpoint
{
  dsk_boolean (*add)    (DskTableCheckpoint *cp,
                         unsigned            key_length,
                         const uint8_t      *key_data,
                         unsigned            value_length,
                         const uint8_t      *value_data,
                         DskError          **error);
  dsk_boolean (*sync)   (DskTableCheckpoint *cp,
		         DskError          **error);
  dsk_boolean (*close)  (DskTableCheckpoint *cp,
		         DskError          **error);
  void        (*destroy)(DskTableCheckpoint *cp);
};
