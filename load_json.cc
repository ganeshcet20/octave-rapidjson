// Copyright (C) 2017 Andreas Weber <andy@josoansi.de>
//
// GNU Octave wrapper around RapidJSON (http://rapidjson.org/)
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License as
// published by the Free Software Foundation; either version 3 of the
// License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
// General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, see
// <http://www.gnu.org/licenses/>.

#include <octave/oct.h>
#include <octave/oct-map.h>
#include <octave/Cell.h>

#include <vector>
#include "dynContainer.h"

#include "rapidjson/document.h"
#include "rapidjson/reader.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/error/en.h"

//#define DEBUG

#ifdef DEBUG
#include <iomanip>
#define DBG_MSG2(d, a, b) for (int __k__ = 0; __k__ < d; ++__k__) std::cout << "-";\
                   std::cout << std::setw (15 - d) << std::left\
                             << __FILE__ << ":"\
                             << std::setw (14) << __FUNCTION__ << ":"\
                             << std::setw (4) << __LINE__ << " ";\
                   std::cout << a << " "\
                             << b << std::endl
#define DBG_MSG1(d, a) DBG_MSG2(d, a, "")
#else
#define DBG_MSG2(d, a, b)
#define DBG_MSG1(d, a)
#endif


class parse_state
{
public:

  parse_state (int depth)
    : _depth (depth),
      is_json_array (0),
      array (NULL)
  {
    DBG_MSG2(_depth, "constructor", _depth);
  }

  ~parse_state ()
  {
    DBG_MSG1(_depth, "destructor");
    delete (array);
  }

  octave_scalar_map result;

  void push_back (octave_value v)
  {
    DBG_MSG2(_depth, "is_json_array=", is_json_array);

    if (is_json_array)
      {
        array->append_value (v);
      }
    else
      {
        result.contents (_key) = v;
      }
  }

  void key (const char* str)
  {
    DBG_MSG1(_depth, str);
    _key = str;
  }

  //~ void start_object ()
  //~ {
    //~ DBG_MSG1(_depth, "");
  //~ }

  void start_array ()
  {
    DBG_MSG1(_depth, "");
    is_json_array = true;
    //array_items = 0;
    
    if (! array)
      array = new dynContainer;
    array->ob ();
  }

  void end_array ()
  {
    DBG_MSG1(_depth, "");
    //DBG_MSG2(_depth, "array_items = ", array_items);
    //assert (elementCount == ps.back().array_items);
    is_json_array = false;

    array->cb ();

    if (array->get_depth () == 0)
      {
        result.contents (_key) = array->get_array ();
        delete (array);
        array = NULL;
      }
  }

private:
  int _depth;             // only for debug output
  int array_depth;
  std::string _key;
  bool is_json_array;     // we are between start_array() and end_array()

  dynContainer *array;
};

class JSON_Handler : public rapidjson::BaseReaderHandler<rapidjson::UTF8<>, JSON_Handler>
{
public:

  octave_scalar_map result;
  std::vector <parse_state*> ps;

public:

  bool Null()
  {
    DBG_MSG1 (0, "");
    ps.back()->push_back (Matrix());
    return true;
  }

  bool Bool(bool b)
  {
    DBG_MSG1 (0, b);
    ps.back()->push_back ((b)? true: false);
    return true;
  }

  bool Uint(unsigned u)
  {
    DBG_MSG1 (0, u);
    ps.back()->push_back (octave_uint32 (u));
    return true;
  }

  bool Int(signed i)
  {
    DBG_MSG1 (0, i);
    ps.back()->push_back (octave_int32 (i));
    return true;
  }

  bool Int64(int64_t i)
  {
    DBG_MSG1 (0, i);
    ps.back()->push_back (octave_int64 (i));
    return true;
  }

  bool Uint64(uint64_t u)
  {
    DBG_MSG1 (0, u);
    ps.back()->push_back (octave_uint64(u));
    return true;
  }

  bool Double(double d)
  {
    DBG_MSG1 (0, d);
    ps.back()->push_back (d);
    return true;
  }

  bool String(const char* str, rapidjson::SizeType length, bool copy)
  {
    (void) length;
    (void) copy;
    DBG_MSG2 (0, str, length);
    ps.back()->push_back (str);
    return true;
  }

  bool StartObject()
  {
    DBG_MSG1 (0, "");
    int d = ps.size () + 1;
    ps.push_back (new parse_state(d));
    //ps.back ()->start_object ();
    return true;
  }

  bool Key(const char* str, rapidjson::SizeType length, bool copy)
  {
    (void) copy;
    if (strlen (str) != length)
      error ("octave-rapidjson can't handle strings with zeros");

    ps.back()->key(str);
    return true;
  }

  bool EndObject(rapidjson::SizeType memberCount)
  {
    (void) memberCount;
    int n = ps.size ();
    DBG_MSG2 (0, "ps.size() = ", n);

    if (n > 1)
      ps[n-2]->push_back (ps[n-1]->result);
    else
      result = ps[n-1]->result;

    delete ps.back ();
    ps.pop_back ();

    return true;
  }

  bool StartArray()
  {
    // Sollte JSON mit '[' anfangen, z.B.
    // '[{"a":5}]'
    // oder
    // '[2,3]'
    
    //if (! ps.size())
    //  ps.push_back (new parse_state(ps.size () + 1));
    
    ps.back()->start_array ();
    return true;
  }

  bool EndArray(rapidjson::SizeType elementCount)
  {
    (void) elementCount;
    ps.back()->end_array ();
    return true;
  }
};


DEFUN_DLD (load_json, args,, "load_json (json_str)")
{
  if (args.length () != 1)
    print_usage ();

  JSON_Handler handler;
  rapidjson::Reader reader;
  std::string json = args(0).string_value ();
  DBG_MSG2(0, "json = ", json);

  rapidjson::StringStream ss(json.c_str());
  rapidjson::ParseResult ok = reader.Parse(ss, handler);

  //rapidjson::Document d;
  //rapidjson::ParseResult ok = d.ParseStream(ss);
  
  if (! ok)
    {
      error ("JSON parse error: '%s' at offset %u",
             rapidjson::GetParseError_En (ok.Code()),
             ok.Offset());
    }

  return ovl (handler.result);
}

/*
%!test
%! json = '{ "hello" : "world", "t" : true , "f" : false, "n": null, "i":-123, "u":456, "pi": 3.1416, "li": -765432986, "a":[1, 2, 3, 4], "b": ["foo", 4] } ';
%! r = load_json (json);
%! assert (r.hello, "world")
%! assert (r.t, true)
%! assert (r.f, false)
%! assert (r.n, [])
%! assert (r.i, int32 (-123))
%! assert (r.u, uint32 (456))
%! assert (r.pi, pi, 1e-5)
%! assert (r.a, [1 2 3 4])
%! assert (r.b, {"foo", 4})

%!test
%! json = '{ "a": [[1,2],[3,4]]}';
%! r =load_json (json);
%! assert (r.a, [1 2; 3 4]);

%!test
%! json = '{ "a" : [[[1,2],[3,4]],[[5,6],[7,8.1]]], "b" : [10,20], "c": [100,200] }';
%! r = load_json (json);
%! assert (r.a, cat (3, [1 2; 3 4], [5 6; 7 8.1]));
%! assert (r.b, [10 20]);
%! assert (r.c, [100 200]);

*/
