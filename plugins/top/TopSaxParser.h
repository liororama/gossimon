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
#include <TopFinder.h>

#include <libxml++/libxml++.h>

typedef std::vector<ProcessStatusInfo> processVecT;

class TopSaxParser : public xmlpp::SaxParser
{
public:
  TopSaxParser();
  virtual ~TopSaxParser();

  bool parseTop(std::string str, processVecT &vec);
  
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
    int                 _mlogId;
    std::string         _errMsg;
    
    int                 _procNum;
    bool                _inProcessEntry;
    std::string         _currValueString;
    std::vector<ProcessStatusInfo> _processVec;
    
};



#endif	/* TOPXMLPARSER_H */

