
/* --- producer --- */
typedef struct _DangProducerClass DangProducerClass;
typedef struct _DangProducer DangProducer;
struct _DangProducerClass
{
  DangObjectClass base_class;
  DangValueType *element_type;
  DangFunction *advance;
};
struct _DangProducer
{
  DangObject base_instance;
  dang_boolean has_entry;
  /* value follows */
};
DangValueType *dang_value_type_producer (DangValueType *element_type);

/* --- consumer --- */
typedef struct _DangConsumerClass DangConsumerClass;
typedef struct _DangConsumer DangConsumer;
struct _DangConsumerClass
{
  DangObjectClass base_class;
  DangValueType *element_type;
  DangFunction *consume;
};
struct _DangConsumer
{
  DangObject base_instance;
};
DangValueType *dang_value_type_consumer (DangValueType *element_type);


void dang_io_connect (DangProducer *producer,
                      DangConsumer *consumer);


DangProducer *dang_producer_new_filter (DangProducer *underlying,
                                        DangFunction *function);
