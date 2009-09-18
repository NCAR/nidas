class CancelProcessingException {
 public:
  CancelProcessingException(const char *s) : message(s) {};
  ~CancelProcessingException() {};
 protected:
  const char *message;
};
