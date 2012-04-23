/* 
 * Cluster Logic Ltd - 2011 - All rights reserved
 *
 *  File:   TopXMLParser.cpp
 * Author: lior
 * 
 * Created on November 28, 2011, 1:39 PM
 */

#include "ModuleLogger.h"
#include "TopSaxParser.h"


#include <iostream>

    
    

TopSaxParser::TopSaxParser()
  : xmlpp::SaxParser()
{
    mlog_registerModule("topparser", "Parsing top plugin xml", "topparser");

    mlog_getIndex("topparser", &_mlog_id);
    _procNum = 0;
    _in_process_entry = false;
    _processVec.clear();
}

TopSaxParser::~TopSaxParser()
{

}

bool TopSaxParser::parse(std::string str, processVecT &vec) {
    
    _procNum = 0;
    _in_process_entry = false;
    _processVec.clear();
    
    try {

        set_substitute_entities(true); //
        parse_memory_raw((const unsigned char*) str.c_str(), str.length() + 1);

    } catch (const xmlpp::exception& ex) {
        _err_msg = std::string("libxml++ exception: ") +  std::string(ex.what());
        mlog_error(_mlog_id, _err_msg.c_str());
        return false;
    }

    vec = _processVec;
    return true;
}

void TopSaxParser::on_start_document()
{
  //std::cout << "on_start_document()" << std::endl;
}

void TopSaxParser::on_end_document()
{
  //std::cout << "on_end_document()" << std::endl;
}

void TopSaxParser::on_start_element(const Glib::ustring& name,
                                   const AttributeList& attributes)
{
  mlog_db(_mlog_id, "node name=[%s]\n", name.c_str());
  if(name == "process") {
      mlog_dg(_mlog_id, "Found process start\n");
      _in_process_entry = true;
      ProcessStatusInfo pi;
      _processVec.push_back(pi);
  }
  // Print attributes:
  for(xmlpp::SaxParser::AttributeList::const_iterator iter = attributes.begin(); iter != attributes.end(); ++iter)
  {
    std::cout << "  Attribute " << iter->name << " = " << iter->value << std::endl;
  }
}

void TopSaxParser::on_end_element(const Glib::ustring& name)
{
  //std::cout << "on_end_element()" << std::endl;
  if(name == "process") {
      _procNum++;
      _in_process_entry = false;
  }
  else if(name == "pid") {
      //mlog_dg(_mlog_id, "Element end of pid [%s]\n", _curr_value_string.c_str());
      
      //_processVec[_procNum]._pid = 5;
      _processVec[_procNum]._pid = atoi(_curr_value_string.c_str());
  }
  else if(name == "name") {
      _processVec[_procNum]._command = _curr_value_string;
  }
  else if(name == "uid") {
      _processVec[_procNum]._uid = atoi(_curr_value_string.c_str());
  }
  else if(name == "mem") {
      _processVec[_procNum]._memoryMB = atof(_curr_value_string.c_str());
  }
  else if(name == "memPercent") {
      _processVec[_procNum]._memPercent = atof(_curr_value_string.c_str());
  }
  else if(name == "cpuPercent") {
      _processVec[_procNum]._cpuPercent = atof(_curr_value_string.c_str());
  }
}

void TopSaxParser::on_characters(const Glib::ustring& text)
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

void TopSaxParser::on_comment(const Glib::ustring& text)
{
  //std::cout << "on_comment(): " << text << std::endl;
}

void TopSaxParser::on_warning(const Glib::ustring& text)
{
  //std::cout << "on_warning(): " << text << std::endl;
}

void TopSaxParser::on_error(const Glib::ustring& text)
{
  //std::cout << "on_error(): " << text << std::endl;
}

void TopSaxParser::on_fatal_error(const Glib::ustring& text)
{
  //std::cout << "on_fatal_error(): " << text << std::endl;
}

