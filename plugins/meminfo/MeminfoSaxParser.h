/* 
 * File:   TopXMLParser.h
 * Author: lior
 *
 * Created on November 28, 2011, 1:40 PM
 */

#ifndef TOPXMLPARSER_H
#define	TOPXMLPARSER_H

#include <string>
#include <vector>
#include <Meminfo.h>

#include <libxml++/libxml++.h>


class MeminfoSaxParser : public xmlpp::SaxParser
{
public:
  MeminfoSaxParser();
  virtual ~MeminfoSaxParser();

  bool parse(std::string str, Meminfo &mi);
  
protected:
  //overrides:
  virtual void on_start_document();
  virtual void on_end_document();
  virtual void on_start_element(const Glib::ustring& name,
                                const AttributeList& properties);
  virtual void on_end_element(const Glib::ustring& name);
  virtual void on_characters(const Glib::ustring& characters);
  virtual void on_comment(const Glib::ustring& text);
  virtual void on_warning(const Glib::ustring& text);
  virtual void on_error(const Glib::ustring& text);
  virtual void on_fatal_error(const Glib::ustring& text);

  
private:
    int                 _mlog_id;
    std::string         _err_msg;
    
    bool                _in_meminfo_entry;
    std::string         _curr_value_string;
    Meminfo             _meminfo;
    
};



#endif	/* TOPXMLPARSER_H */

