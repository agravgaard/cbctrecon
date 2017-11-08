/*
 * File automatically generated by
 * gengen 1.2 by Lorenzo Bettini 
 * http://www.gnu.org/software/gengen
 */

#ifndef GROUP_OPTION_GEN_CLASS_H
#define GROUP_OPTION_GEN_CLASS_H

#include <string>
#include <iostream>

using std::string;
using std::ostream;

class group_option_gen_class
{
 protected:
  string Comparison_rule;
  string group_name;
  string group_var_name;
  string number_required;
  string package_var_name;

 public:
  group_option_gen_class()
  {
  }
  
  group_option_gen_class(const string &_Comparison_rule, const string &_group_name, const string &_group_var_name, const string &_number_required, const string &_package_var_name) :
    Comparison_rule (_Comparison_rule), group_name (_group_name), group_var_name (_group_var_name), number_required (_number_required), package_var_name (_package_var_name)
  {
  }

  static void
  generate_string(const string &s, ostream &stream, unsigned int indent)
  {
    if (!indent || s.find('\n') == string::npos)
      {
        stream << s;
        return;
      }

    string::size_type pos;
    string::size_type start = 0;
    string ind (indent, ' ');
    while ( (pos=s.find('\n', start)) != string::npos)
      {
        stream << s.substr (start, (pos+1)-start);
        start = pos+1;
        if (start+1 <= s.size ())
          stream << ind;
      }
    if (start+1 <= s.size ())
      stream << s.substr (start);
  }

  void set_Comparison_rule(const string &_Comparison_rule)
  {
    Comparison_rule = _Comparison_rule;
  }

  void set_group_name(const string &_group_name)
  {
    group_name = _group_name;
  }

  void set_group_var_name(const string &_group_var_name)
  {
    group_var_name = _group_var_name;
  }

  void set_number_required(const string &_number_required)
  {
    number_required = _number_required;
  }

  void set_package_var_name(const string &_package_var_name)
  {
    package_var_name = _package_var_name;
  }

  void generate_group_option(ostream &stream, unsigned int indent = 0);
  
};

#endif // GROUP_OPTION_GEN_CLASS_H
