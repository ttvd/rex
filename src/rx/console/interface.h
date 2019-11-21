#ifndef RX_CONSOLE_INTERFACE_H
#define RX_CONSOLE_INTERFACE_H
#include "rx/console/variable.h"
#include "rx/console/command.h"

namespace rx::console {

struct auto_complete;

struct interface {
  // loads |_file_name| configuration
  static bool load(const char* _file_name);

  // saves configuration to |_file_name|
  static bool save(const char* _file_name);

  // thread type-erased static global variable |_reference| during static
  // initialization into global linked-list representation
  static variable_reference* add_variable_reference(variable_reference* _reference);

  // set variable |_name| with typed value |_value|
  template<typename T>
  static variable_status set_from_value(const string& _name, const T& _value);

  // reset variable |_name|
  template<typename T>
  static void reset_from_reference(variable_reference* _reference);

  // change variable |_name| with string encoded |_value|
  static variable_status change(const string& _name, const string& _value);

  // reset variable |_name|
  static bool reset(const string& _name);

  // get variable |_name|
  static variable_reference* get_from_name(const string& _name);

  static bool execute(const string& _contents);

  static const vector<string>& lines();

  static void add_command(const string& _name, const char* _signature,
    function<bool(const vector<command::argument>&)>&& _function);

  template<typename... Ts>
  static void print(const char* _format, Ts&&... _arguments);
  static void write(const string& _message);
  static void clear();

  static vector<string> complete(const string& _prefix);

private:
  // parse string |_contents| into typed value |value_|
  template<typename T>
  static variable_status parse_string(const string& _contents, T& value_);

  // set variable |_name| with value |_value|
  template<typename T>
  static variable_status set_from_name_and_value(const string& _name, const T& _value);

  // set variable |_reference| with value |_value|
  template<typename T>
  static variable_status set_from_reference_and_value(variable_reference* _reference, const T& _value);


  // set variable |_name| with string encoded value |_string_value|
  template<typename T>
  static variable_status set_from_name_and_string(const string& _name, const string& _string_value);

  // set variable |_reference| with string encoded value alue |_string_value|
  template<typename T>
  static variable_status set_from_reference_and_string(variable_reference* _reference, const string& _string_value);

  // merge-sort variable references in alphabetical order
  static variable_reference* split(variable_reference* _reference);
  static variable_reference* merge(variable_reference* _lhs, variable_reference* _rhs);
  static variable_reference* sort(variable_reference* _reference);
};

template<typename... Ts>
inline void interface::print(const char* _format, Ts&&... _arguments) {
  write(string::format(_format, utility::forward<Ts>(_arguments)...));
}

} // namespace rx::console

#endif // RX_CONSOLE_INTERFACE_H
