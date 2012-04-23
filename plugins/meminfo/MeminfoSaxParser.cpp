/* 
 * Cluster Logic Ltd - 2011 - All rights reserved
 *
 *  File:   TopXMLParser.cpp
 * Author: lior
 * 
 * Created on November 28, 2011, 1:39 PM
 */

#include "ModuleLogger.h"

#include "MeminfoSaxParser.h"


#include <iostream>

    
    

MeminfoSaxParser::MeminfoSaxParser()
  : xmlpp::SaxParser()
{
    mlog_registerModule("meminfoparser", "Parsing top plugin xml", "meminfoparser");
    mlog_getIndex("meminfoparser", &_mlog_id);
    
}

MeminfoSaxParser::~MeminfoSaxParser()
{

}

bool MeminfoSaxParser::parse(std::string str, Meminfo &mi) {

    try {

        set_substitute_entities(true); //
        parse_memory_raw((const unsigned char*) str.c_str(), str.length() + 1);

    } catch (const xmlpp::exception& ex) {
        _err_msg = std::string("libxml++ exception: ") +  std::string(ex.what());
        mlog_error(_mlog_id, _err_msg.c_str());
        return false;
    }

    mi = _meminfo;
    return true;
}

void MeminfoSaxParser::on_start_document()
{
  //std::cout << "on_start_document()" << std::endl;
}

void MeminfoSaxParser::on_end_document()
{
  //std::cout << "on_end_document()" << std::endl;
}

void MeminfoSaxParser::on_start_element(const Glib::ustring& name,
                                   const AttributeList& attributes)
{
  mlog_db(_mlog_id, "node name=[%s]\n", name.c_str());
  if(name == "meminfo") {
      mlog_dg(_mlog_id, "Found meminfo start\n");
      _in_meminfo_entry = true;
      
  }
  // Print attributes:
  for(xmlpp::SaxParser::AttributeList::const_iterator iter = attributes.begin(); iter != attributes.end(); ++iter)
  {
    std::cout << "  Attribute " << iter->name << " = " << iter->value << std::endl;
  }
}

void MeminfoSaxParser::on_end_element(const Glib::ustring& name)
{
  //std::cout << "on_end_element()" << std::endl;
  if(name == "meminfo") {
      _in_meminfo_entry = false;
  }
  else if(name == "total") {
      _meminfo._total_kb  = atoi(_curr_value_string.c_str());
  }
  else if(name == "free") {
      _meminfo._free_kb  = atoi(_curr_value_string.c_str());
  }
  else if(name == "buffers") {
      _meminfo._buffers_kb  = atoi(_curr_value_string.c_str());
  }
  else if(name == "cached") {
      _meminfo._cached_kb  = atoi(_curr_value_string.c_str());
  }
  else if(name == "dirty") {
      _meminfo._dirty_kb  = atoi(_curr_value_string.c_str());
  }
  else if(name == "mlocked") {
      _meminfo._mlocked_kb  = atoi(_curr_value_string.c_str());
  }
  
}

void MeminfoSaxParser::on_characters(const Glib::ustring& text)
{
  //std::cout << "on_characters(): " << text << std::endl;
    mlog_dy(_mlog_id, "on_characters(): [%s]\n", text.c_str());
    if (text.find_first_not_of (" \t\n") == text.npos) 
        return;
    if(text == "\n")
        return;
    
    mlog_dy(_mlog_id, "------> setting value to  [%s]\n", text.c_str());
        _curr_value_string = text;
    
}

void MeminfoSaxParser::on_comment(const Glib::ustring& text)
{
  //std::cout << "on_comment(): " << text << std::endl;
}

void MeminfoSaxParser::on_warning(const Glib::ustring& text)
{
  //std::cout << "on_warning(): " << text << std::endl;
}

void MeminfoSaxParser::on_error(const Glib::ustring& text)
{
  //std::cout << "on_error(): " << text << std::endl;
}

void MeminfoSaxParser::on_fatal_error(const Glib::ustring& text)
{
  //std::cout << "on_fatal_error(): " << text << std::endl;
}

