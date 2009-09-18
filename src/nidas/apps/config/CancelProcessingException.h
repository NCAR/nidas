class CancelProcessingException {
 public:
  CancelProcessingException(const char *s) { message = new std::string(s); }
  CancelProcessingException(std::string & s) { message = new std::string(s); }
  ~CancelProcessingException() {}

  const char *what() const { return message->c_str(); }
  std::string getMessage() const { return *message; }

 protected:
  std::string *message;
};
