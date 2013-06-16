#ifndef JSON_SERIALIZATION_H
#define JSON_SERIALIZATION_H

#include <map>
#include <type_traits>
#include <vector>
#include <string>
#include <memory>
#include <stack>
#include <utility>
#include <boost/variant/static_visitor.hpp>
#include "rapidjson/rapidjson.h"
#include "rapidjson/reader.h"
#include "rapidjson/writer.h"

//tagged json (need to deal with some sort of work buffer or abstract class + allocation mechanism
//curently not doing any memory managment since everything is assumed to be allocated on the stack
//might just need to bite the bullet and implement unique_ptr members then we can stack alloc the base
//as a temp buffer and then allocate the real result in a unique_ptr
//this is to solve the reddit problem where the tag field isnt the first field that gets read in


//an alternate option to the above is to add variant support this would unfortunately mean all objects
//in a set would take up the size of their largest member but, the amount of active data at any given
//time is probably low. this leads to an object layout where the outer object is layed out like this
//struct adt { int id; string tag; variant<post,link,comment,more,listing,subreddit> adt_data};
//not sure how to look for variant type when running reflection over. variant has a typedef named 'types'
//its an mpl sequence so it should be possible to iterate over it and generate whatever is needed
//since variant must always have a valid value, one of the variants must be nil. All types must be
//default constructable, need to make a map of pre constructed variants to set equal to

struct BaseReaderHandler
{
  virtual void Null() = 0;
  virtual void Bool(bool) = 0;
  virtual void Int(int) = 0;
  virtual void Uint(unsigned) = 0;
  virtual void Int64(int64_t) = 0;
  virtual void Uint64(uint64_t) = 0;
  virtual void Double(double) = 0;
  virtual void String(const char*, size_t, bool) = 0;
  virtual std::unique_ptr<BaseReaderHandler> && StartObject(std::unique_ptr<BaseReaderHandler> &&) = 0;
  virtual void EndObject(size_t) = 0;
  virtual std::unique_ptr<BaseReaderHandler> && StartArray(std::unique_ptr<BaseReaderHandler> &&) = 0;
  virtual void EndArray(size_t)  = 0;
};

namespace rapidjson
{
  class BaseWriter;
}


template <typename PrimativeDeserializationVisitor, typename ObjectDeserializationVisitor, typename ReflectableObject>
std::unique_ptr<BaseReaderHandler> DeserializeHandler(ReflectableObject& reflectable);
template <typename PrimativeDeserializationVisitor, typename ObjectDeserializationVisitor, typename ReflectableArray>
std::unique_ptr<BaseReaderHandler> DeserializeArrayHandler(ReflectableArray& reflectable);

//these visitors need to be wrapped in a type so we can pass them unspecialized to DeserializeHandler
struct DeserializationReflectableVisitor
{
  template<typename T, typename ReflectableObject>
  class type : public boost::static_visitor<>
  {
    const T& _value;
    ReflectableObject& _reflectable;
  public:
    type(const T& value, ReflectableObject& reflectable) : _value(value), _reflectable(reflectable) {}

    template<typename Tn>
    auto impl(Tn ReflectableObject::* data, int) const -> decltype(std::declval<typename std::remove_pointer<Tn>::type::inflatable>(), std::remove_pointer<Tn>::type::inflatable::inflate(std::declval<T>()),  void())
    {
      _reflectable.*data = std::remove_pointer<Tn>::type::inflatable::inflate(_value);
    }

    template<typename Tn>
    void impl(Tn ReflectableObject::* data, char) const
    { 
    }

    template<typename Tn>
    auto impl(Tn ReflectableObject::* data, int) const -> decltype(std::declval<typename std::enable_if<std::is_same<T, Tn>::value>::type>(), void())
    { 
      _reflectable.*data = _value;
    }
    
    template<typename Tn>
    auto operator()(Tn ReflectableObject::* data) const -> decltype(std::declval<type<T,ReflectableObject>*>()->impl(data, 0))
    {
      impl(data, 0);
    }

  };
};

struct DeserializationObjectReflectableVisitor
{

  class ReflectableVariantVisitor : public boost::static_visitor<>
  {
    std::unique_ptr<BaseReaderHandler>& _value;
  public:
    ReflectableVariantVisitor(std::unique_ptr<BaseReaderHandler>& value) : _value(value) { }
    template<typename NestedTn>
    void operator()(NestedTn& actualData) const
    {
      _value = DeserializeHandler<DeserializationReflectableVisitor, DeserializationObjectReflectableVisitor>(actualData);
    }
  };

  template<typename ReflectableObject>
  class type : public boost::static_visitor<>
  {
    std::unique_ptr<BaseReaderHandler>& _value;
    ReflectableObject& _reflectable;
  public:
    type(std::unique_ptr<BaseReaderHandler>& value, ReflectableObject& reflectable) : _value(value), _reflectable(reflectable) {}

    template<typename Tn>
    auto impl(Tn ReflectableObject::* data, long) const -> decltype(std::declval<typename Tn::reflectable>(), void())
    {
      _value = DeserializeHandler<DeserializationReflectableVisitor, DeserializationObjectReflectableVisitor>(_reflectable.*data);
    }

    template<typename Tn, typename ReflObj>
    auto impl(Tn ReflObj::* data, int) const -> decltype(std::declval<typename ReflObj::reflectable>().json_tag_map, void())
    {
      boost::apply_visitor(ReflectableVariantVisitor(_value), _reflectable.*data);
    }
    
    template<typename Tn>
    auto impl(Tn ReflectableObject::* data, unsigned int) const -> decltype(std::declval<std::remove_pointer<Tn>::type::inflatable>(), void())
    {
      _value = DeserializeHandler<DeserializationReflectableVisitor, DeserializationObjectReflectableVisitor>(std::remove_pointer<Tn>::type::inflatable::inflate(_reflectable.*data));
    }

    template<typename Tn>
    void impl(Tn ReflectableObject::* data, unsigned char) const { }

    template<typename Tn>
    auto operator()(Tn ReflectableObject::* data) const -> decltype(std::declval<type<ReflectableObject>*>()->impl(data, 0))
    {
      impl(data, 0);
    }

    void operator()(int ReflectableObject::* data) const { }
    void operator()(std::string ReflectableObject::* data) const { }
    void operator()(unsigned ReflectableObject::* data) const { }
    void operator()(uint64_t ReflectableObject::* data) const { }
    void operator()(int64_t ReflectableObject::* data) const { }
    void operator()(double ReflectableObject::* data) const { }
    void operator()(bool ReflectableObject::* data) const { }
  };
};

struct DeserializationPrimativeArrayReflectableVisitor
{
  template<typename T, typename ReflectableObject>
  class type : public boost::static_visitor<>
  {
    const T& _value;
    ReflectableObject& _reflectable;
  public:
    type(const T& value, ReflectableObject& reflectable) : _value(value), _reflectable(reflectable) {}

    template<typename Tn>
    void operator()(Tn ReflectableObject::* data) const { }
    void operator()(T ReflectableObject::* data) const
    {
      (_reflectable.*data).push_back(_value);
    }
  };
};

struct DeserializationObjectArrayReflectableVisitor
{
  template<typename ReflectableObject>
  class type : public boost::static_visitor<>
  {
    std::unique_ptr<BaseReaderHandler>& _value;
    ReflectableObject& _reflectable;
  public:
    type(std::unique_ptr<BaseReaderHandler>& value, ReflectableObject& reflectable) : _value(value), _reflectable(reflectable) {}

    template<typename Tn>
    auto impl(Tn ReflectableObject::* data, int) const -> decltype(Tn::reflectable, void())
    {
      (_reflectable.*data).push_back(Tn());
      _value = DeserializeHandler<DeserializationReflectableVisitor, DeserializationObjectReflectableVisitor>(_reflectable.back());
    }

    template<typename Tn>
    void impl(Tn ReflectableObject::* data, long) const { }

    template<typename Tn>
    auto operator()(Tn ReflectableObject::* data) const -> decltype(impl(data, 0))
    {
      impl(data, 0);
    }

    void operator()(int ReflectableObject::* data) const { }
    void operator()(std::string ReflectableObject::* data) const { }
    void operator()(unsigned ReflectableObject::* data) const { }
    void operator()(uint64_t ReflectableObject::* data) const { }
    void operator()(int64_t ReflectableObject::* data) const { }
    void operator()(double ReflectableObject::* data) const { }
    void operator()(bool ReflectableObject::* data) const { }
  };
};

struct DeserializationArrayReflectableVisitor
{
  template<typename ReflectableObject>
  class type : public boost::static_visitor<>
  {
    std::unique_ptr<BaseReaderHandler>& _value;
    ReflectableObject& _reflectable;
  public:
    type(std::unique_ptr<BaseReaderHandler>& value, ReflectableObject& reflectable) : _value(value), _reflectable(reflectable) {}

    //we're a vector or something like that
    template<typename Tn, typename ReflectableObject2>
    auto impl(Tn ReflectableObject2::* data, long) const -> decltype(std::declval<ReflectableObject2>().push_back(Tn()), void())
    {
      _reflectable.push_back(Tn());
      _value = DeserializeArrayHandler<DeserializationReflectableVisitor, DeserializationObjectReflectableVisitor>(_reflectable.back());
    }
    //we're a nested vector or something like that
    template<typename Tn>
    auto impl(Tn ReflectableObject::* data, int) const -> decltype(Tn().push_back(Tn::value_type), void())
    {
      (_reflectable.*data).push_back(Tn());
      _value = DeserializeArrayHandler<DeserializationReflectableVisitor, DeserializationObjectReflectableVisitor>((_reflectable.*data));
    }

    template<typename Tn>
    void impl(Tn ReflectableObject::* data, short) const { }

    template<typename Tn>
    auto  operator()(Tn ReflectableObject::* data) const -> decltype(std::declval<type<ReflectableObject>*>()->impl(data, 0))
    {
      impl(data, 0);
    }

    void operator()(int ReflectableObject::* data) const { }
    void operator()(std::string ReflectableObject::* data) const { }
    void operator()(unsigned ReflectableObject::* data) const { }
    void operator()(uint64_t ReflectableObject::* data) const { }
    void operator()(int64_t ReflectableObject::* data) const { }
    void operator()(double ReflectableObject::* data) const { }
    void operator()(bool ReflectableObject::* data) const { }
  };
};

template<typename Writer, typename ReflectableObject>
class SerializationReflectableVisitor : public boost::static_visitor<>
{
  Writer& _writer;
  ReflectableObject& _reflectable;
public:
  SerializationReflectableVisitor(Writer& writer, ReflectableObject& reflectable) : _writer(writer), _reflectable(reflectable) {}
  void operator()(int ReflectableObject::* data) const
  {
    _writer.Int(_reflectable.*data);
  }

  void operator()(unsigned ReflectableObject::* data) const
  {
    _writer.Uint(_reflectable.*data);
  }

  void operator()(int64_t ReflectableObject::* data) const
  {
    _writer.Int64(_reflectable.*data);
  }

  void operator()(uint64_t ReflectableObject::* data) const
  {
    _writer.Uint64(_reflectable.*data);
  }

  void operator()(bool ReflectableObject::* data) const
  {
    _writer.Bool(_reflectable.*data);
  }

  void operator()(double ReflectableObject::* data) const
  {
    _writer.Double(_reflectable.*data);
  }

  void operator()(std::string ReflectableObject::* data) const
  {
    _writer.String((_reflectable.*data).c_str(), (_reflectable.*data).size());
  }
  
  
  void operator()(int data) const
  {
    _writer.Int(data);
  }

  void operator()(unsigned data) const
  {
    _writer.Uint(data);
  }

  void operator()(int64_t data) const
  {
    _writer.Int64(data);
  }

  void operator()(uint64_t data) const
  {
    _writer.Uint64(data);
  }
  

  template<typename Tn>
  auto impl(Tn ReflectableObject::* data, unsigned int) const -> decltype(std::declval<typename Tn::iterator>(), void())
  {
    SerializeArray(_writer, (_reflectable.*data));
  }

  template<typename Tn>
  auto impl(Tn ReflectableObject::* data, long) const -> decltype(std::declval<typename Tn::reflectable>(), void())
  {
    Serialize(_writer, (_reflectable.*data));
  }
  
  template<typename Tn>
  auto impl(Tn ReflectableObject::* data, int) const -> decltype(std::declval<typename std::remove_pointer<Tn>::type::inflatable>(), void())
  {
    this->operator()(std::remove_pointer<Tn>::type::inflatable::deflate(_reflectable.*data));
  }

  class VariantSerializationVisitor : public boost::static_visitor<>
  {
    Writer& _writer;
  public:
    VariantSerializationVisitor(Writer& writer) : _writer(writer) {}
    template<typename VariantTn>
    void operator()(VariantTn& actualData) const
    {
      Serialize(_writer, actualData);
    }
  };

  template<typename Tn>
  auto impl(Tn ReflectableObject::* data, int) const ->
  decltype(std::declval<typename Tn::types>(), void())
  {
    boost::apply_visitor(VariantSerializationVisitor(_writer), _reflectable.*data);
  }

  template<typename Tn>
  auto operator()(Tn ReflectableObject::* data) const -> decltype(((SerializationReflectableVisitor<Writer, ReflectableObject>*)nullptr)->impl(data, 0))
  {
    impl(data, 0);
  }
};

template<typename Writer>
class SerializationArrayReflectableVisitor : public boost::static_visitor<>
{
  Writer& _writer;
public:
  SerializationArrayReflectableVisitor(Writer& writer) : _writer(writer) {}
  void operator()(int data) const
  {
    _writer.Int(data);
  }

  void operator()(unsigned data) const
  {
    _writer.Uint(data);
  }

  void operator()(int64_t data) const
  {
    _writer.Int64(data);
  }

  void operator()(uint64_t data) const
  {
    _writer.Uint64(data);
  }

  void operator()(bool data) const
  {
    _writer.Bool(data);
  }

  void operator()(double data) const
  {
    _writer.Double(data);
  }

  void operator()(const std::string& data) const
  {
    _writer.String(data.c_str(), data.size());
  }

  template<typename Tn>
  auto impl(Tn& data, long) const -> decltype(std::declval<typename Tn::iterator>(), void())
  {
    SerializeArray(_writer, data);
  }

  template<typename Tn>
  auto impl(Tn& data, int) const -> decltype(std::declval<typename Tn::reflectable>(), void())
  {
    Serialize(_writer, data);
  }
  
  template<typename Tn>
  auto impl(Tn& data, unsigned int) const -> decltype(std::declval<std::remove_pointer<Tn>::inflatable>(), void())
  {
    Serialize(_writer, data);
  }

  class VariantSerializationVisitor : public boost::static_visitor<>
  {
    Writer& _writer;
  public:
    VariantSerializationVisitor(Writer& writer) : _writer(writer) {}
    template<typename VariantTn>
    void operator()(VariantTn& actualData) const
    {
      Serialize(_writer, actualData);
    }
  };

  template<typename Tn>
  auto impl(Tn& data, short) const ->
  decltype(std::declval<VariantSerializationVisitor>()(data), void())
  {
    boost::apply_visitor(VariantSerializationVisitor(_writer), data);
  }

  template<typename Tn>
  auto operator()(Tn& data) const -> decltype(std::declval<SerializationArrayReflectableVisitor<Writer>*>()->impl(data, 0))
  {
    impl(data, 0);
  }
};

template <typename Writer, typename ReflectableObject>
void Serialize(Writer& writer, ReflectableObject& reflectable)
{
  static typename ReflectableObject::reflectable reflection_map;
  writer.StartObject();

  SerializationReflectableVisitor<Writer, ReflectableObject> fieldVisitor(writer, reflectable);

  for(auto pair : reflection_map.noLookup)
  {
    writer.String(pair.first.c_str(), pair.first.size());
    boost::apply_visitor(fieldVisitor, pair.second);
  }

  writer.EndObject();
}

template <typename Writer, typename ReflectableArray>
void SerializeArray(Writer& writer, ReflectableArray& reflectableArray)
{
  writer.StartArray();

  SerializationArrayReflectableVisitor<Writer> fieldVisitor(writer);

  for(auto value : reflectableArray)
  {
    fieldVisitor(value);
  }

  writer.EndArray();
}

template<typename Stream>
void Deserialize(Stream& stream, std::unique_ptr<BaseReaderHandler> && handler)
{
  struct ReaderHandler
  {
    std::stack<std::unique_ptr<BaseReaderHandler>> _handlerStack;
    ReaderHandler(std::unique_ptr<BaseReaderHandler> && handler)
    {
      _handlerStack.push(std::move(handler));
    }

    void Null() { _handlerStack.top()->Null(); }
    void Bool(bool value) { _handlerStack.top()->Bool(value); }
    void Int(int value) { _handlerStack.top()->Int(value); }
    void Uint(unsigned value) { _handlerStack.top()->Uint(value); }
    void Int64(int64_t value) { _handlerStack.top()->Int64(value); }
    void Uint64(uint64_t value) { _handlerStack.top()->Uint64(value); }
    void Double(double value) { _handlerStack.top()->Double(value); }
    void String(const char* value, size_t length, bool b) { _handlerStack.top()->String(value, length, b); }
    void StartObject()
    {
      _handlerStack.push(_handlerStack.top()->StartObject(std::unique_ptr<BaseReaderHandler>(nullptr)));

      if(_handlerStack.top() == nullptr)
        _handlerStack.pop();
    }
    void EndObject(size_t size)
    {
      _handlerStack.top()->EndObject(size);
      _handlerStack.pop();
    }
    void StartArray()
    {
      _handlerStack.push(_handlerStack.top()->StartArray(std::unique_ptr<BaseReaderHandler>(nullptr)));

      if(_handlerStack.top() == nullptr)
        _handlerStack.pop();
    }
    void EndArray(size_t size)
    {
      _handlerStack.top()->EndArray(size);
      _handlerStack.pop();
    }
  };
  rapidjson::Reader reader;
  ReaderHandler rHandler(std::move(handler));
  reader.Parse<0>(stream, rHandler);
}

template <typename PrimativeDeserializationVisitor, typename ObjectDeserializationVisitor, typename ReflectableArray>
std::unique_ptr<BaseReaderHandler> DeserializeArrayHandler(ReflectableArray& reflectable)
{
  struct ReaderHandler : public BaseReaderHandler
  {
    ReaderHandler(ReflectableArray& reflectable) : _reflectable(reflectable) { }
    ReflectableArray& _reflectable;
    std::string _tempStringValue;

    void Default() {}
    virtual void Null() {  }
    virtual void Bool(bool value)
    {
      typename PrimativeDeserializationVisitor::template type<bool, ReflectableArray>(value, _reflectable)(static_cast<bool ReflectableArray::*>(nullptr));
    }
    virtual void Int(int value)
    {
      typename PrimativeDeserializationVisitor::template type<int, ReflectableArray>(value, _reflectable)(static_cast<int ReflectableArray::*>(nullptr));
    }
    virtual void Uint(unsigned value)
    {
      typename PrimativeDeserializationVisitor::template type<unsigned, ReflectableArray>(value, _reflectable)(static_cast<unsigned ReflectableArray::*>(nullptr));
    }
    virtual void Int64(int64_t value)
    {
      typename PrimativeDeserializationVisitor::template type<int64_t, ReflectableArray>(value, _reflectable)(static_cast<int64_t ReflectableArray::*>(nullptr));
    }
    virtual void Uint64(uint64_t value)
    {
      typename PrimativeDeserializationVisitor::template type<uint64_t, ReflectableArray>(value, _reflectable)(static_cast<uint64_t ReflectableArray::*>(nullptr));
    }
    virtual void Double(double value)
    {
      typename PrimativeDeserializationVisitor::template type<uint64_t, ReflectableArray>(value, _reflectable)(static_cast<uint64_t ReflectableArray::*>(nullptr));
    }
    virtual void String(const char* value, size_t length, bool)
    {
      typename PrimativeDeserializationVisitor::template type<std::string, ReflectableArray>(_tempStringValue.assign(value, length), _reflectable)(static_cast<uint64_t ReflectableArray::*>(nullptr));
    }
    virtual std::unique_ptr<BaseReaderHandler> && StartObject(std::unique_ptr<BaseReaderHandler> && result)
    {
      typename ObjectDeserializationVisitor::template type<ReflectableArray>(result, _reflectable)(static_cast<typename ReflectableArray::value_type ReflectableArray::*>(nullptr));
      return std::move(result);
    }
    virtual void EndObject(size_t) { Default(); }
    virtual std::unique_ptr<BaseReaderHandler> && StartArray(std::unique_ptr<BaseReaderHandler> && result)
    {
      typename DeserializationArrayReflectableVisitor::template type<ReflectableArray>(result, _reflectable)(static_cast<typename ReflectableArray::value_type ReflectableArray::*>(nullptr));

      return std::move(result);
    }
    virtual void EndArray(size_t) { Default(); }
  };

  return std::unique_ptr<BaseReaderHandler>(new ReaderHandler(reflectable));
}

template <typename PrimativeDeserializationVisitor, typename ObjectDeserializationVisitor, typename ReflectableObject>
std::unique_ptr<BaseReaderHandler> DeserializeHandler(ReflectableObject& reflectable, long)
{
  static typename ReflectableObject::reflectable reflection_map;
  struct ReaderHandler : public BaseReaderHandler
  {
    ReaderHandler(ReflectableObject& reflectable, typename ReflectableObject::reflectable& reflectionMap) : _reflectable(reflectable),
      _lookingForName(true), _reflection_map(reflectionMap) { }
    ReflectableObject& _reflectable;
    typename ReflectableObject::reflectable& _reflection_map;
    bool _lookingForName;
    std::string _currentName;
    std::string _tempStringValue;

    void Default() {}
    virtual void Null() { _lookingForName = true; }
    virtual void Bool(bool value)
    {
      /*if(_inArray)
        typename PrimativeDeserializationVisitor::type<bool, ReflectableObject>(value, _reflectable)(static_cast<bool ReflectableObject::*>(nullptr));
      else*/
      boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<bool, ReflectableObject>(value, _reflectable), _reflection_map.map[_currentName]);
      _lookingForName = true;
    }
    virtual void Int(int value)
    {
      boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<int, ReflectableObject>(value, _reflectable), _reflection_map.map[_currentName]);
      _lookingForName = true;
    }
    virtual void Uint(unsigned value)
    {
      auto reflectionEntry = _reflection_map.map[_currentName];
      boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<int, ReflectableObject>(value, _reflectable), reflectionEntry);
      boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<unsigned, ReflectableObject>(value, _reflectable), reflectionEntry);
      _lookingForName = true;
    }
    virtual void Int64(int64_t value)
    {
      auto reflectionEntry = _reflection_map.map[_currentName];
      boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<int64_t, ReflectableObject>(value, _reflectable), reflectionEntry);
      _lookingForName = true;
    }
    virtual void Uint64(uint64_t value)
    {
      auto reflectionEntry = _reflection_map.map[_currentName];
      boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<int64_t, ReflectableObject>(value, _reflectable), reflectionEntry);
      boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<uint64_t, ReflectableObject>(value, _reflectable), reflectionEntry);
      _lookingForName = true;
    }
    virtual void Double(double value)
    {
      boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<double, ReflectableObject>(value, _reflectable), _reflection_map.map[_currentName]);
      _lookingForName = true;
    }
    virtual void String(const char* value, size_t length, bool)
    {
      if(!_lookingForName)
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<std::string, ReflectableObject>(_tempStringValue.assign(value, length), _reflectable), _reflection_map.map[_currentName]);

      else
        _currentName.assign(value, length);

      _lookingForName = !_lookingForName;

    }
    virtual std::unique_ptr<BaseReaderHandler> && StartObject(std::unique_ptr<BaseReaderHandler> && result)
    {
      if(!_lookingForName)
        boost::apply_visitor(typename ObjectDeserializationVisitor::template type<ReflectableObject>(result, _reflectable), _reflection_map.map[_currentName]);

      _lookingForName = true;

      return std::move(result);
    }
    virtual void EndObject(size_t) { Default(); }
    virtual std::unique_ptr<BaseReaderHandler> && StartArray(std::unique_ptr<BaseReaderHandler> && result)
    {
      if(!_lookingForName)
        boost::apply_visitor(typename DeserializationArrayReflectableVisitor::template type<ReflectableObject>(result, _reflectable), _reflection_map.map[_currentName]);

      _lookingForName = true;

      return std::move(result);
    }
    virtual void EndArray(size_t) { Default(); }
  };

  return std::unique_ptr<BaseReaderHandler>(new ReaderHandler(reflectable, reflection_map));
}

template <typename PrimativeDeserializationVisitor, typename ObjectDeserializationVisitor, typename ReflectableObject>
class VariantDeserializationVisitor : public boost::static_visitor<>
{
  std::unique_ptr<BaseReaderHandler>& _result;
public:
  VariantDeserializationVisitor(std::unique_ptr<BaseReaderHandler>& result) : _result(result) {}
  template<typename VariantTn>
  void operator()(VariantTn& actualData) const
  {
    _result = DeserializeHandler<DeserializationReflectableVisitor, DeserializationObjectReflectableVisitor>(actualData);
  }
};

//Specialization for objects that have their type specified by a tag_map
//this ends up having to contain its own BaseReaderHandler, since there isnt actually
//a new object being created from a json perspective but on the c++ side
//the object is totally different. Its not a stack though because all fields that follow the tag
//must be fields of the variant instead of the original object
template <typename PrimativeDeserializationVisitor, typename ObjectDeserializationVisitor, typename ReflectableObject>
auto DeserializeHandler(ReflectableObject& reflectable, int) -> decltype(std::declval<typename ReflectableObject::reflectable::json_tag_map>(), std::unique_ptr<BaseReaderHandler>())
{
  static typename ReflectableObject::reflectable reflection_map;
  struct ReaderHandler : public BaseReaderHandler
  {
    ReaderHandler(ReflectableObject& reflectable) : _reflectable(reflectable), _lookingForName(true) { }
    ReflectableObject& _reflectable;

    bool _lookingForName;
    std::string _currentName;
    std::string _tempStringValue;
    std::unique_ptr<BaseReaderHandler> _variantReaderHandler;

    void Default() {}
    virtual void Null() { _lookingForName = true; }
    virtual void Bool(bool value)
    {
      if(_variantReaderHandler != nullptr)
        _variantReaderHandler->Bool(value);
      else
      {
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<bool, ReflectableObject>(value, _reflectable), reflection_map.map[_currentName]);
        _lookingForName = true;
      }
    }
    virtual void Int(int value)
    {
      if(_variantReaderHandler != nullptr)
        _variantReaderHandler->Int(value);
      else
      {
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<int, ReflectableObject>(value, _reflectable), reflection_map.map[_currentName]);
        _lookingForName = true;
      }
    }
    virtual void Uint(unsigned value)
    {
      if(_variantReaderHandler != nullptr)
        _variantReaderHandler->Uint(value);
      else
      {
        auto reflectionEntry = reflection_map.map[_currentName];
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<int, ReflectableObject>(value, _reflectable), reflectionEntry);
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<unsigned, ReflectableObject>(value, _reflectable), reflectionEntry);
        _lookingForName = true;
      }
    }
    virtual void Int64(int64_t value)
    {
      if(_variantReaderHandler != nullptr)
        _variantReaderHandler->Int64(value);
      else
      {
        auto reflectionEntry = reflection_map.map[_currentName];
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<int64_t, ReflectableObject>(value, _reflectable), reflectionEntry);
        _lookingForName = true;
      }
    }
    virtual void Uint64(uint64_t value)
    {
      if(_variantReaderHandler != nullptr)
        _variantReaderHandler->Uint64(value);
      else
      {
        auto reflectionEntry = reflection_map.map[_currentName];
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<int64_t, ReflectableObject>(value, _reflectable), reflectionEntry);
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<uint64_t, ReflectableObject>(value, _reflectable), reflectionEntry);
        _lookingForName = true;
      }
    }
    virtual void Double(double value)
    {
      if(_variantReaderHandler != nullptr)
        _variantReaderHandler->Double(value);
      else
      {
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<double, ReflectableObject>(value, _reflectable), reflection_map.map[_currentName]);
        _lookingForName = true;
      }
    }

    virtual void String(const char* value, size_t length, bool unnamedBool)
    {
      if(_variantReaderHandler != nullptr)
        _variantReaderHandler->String(value, length, unnamedBool);
      else
      {
        if(!_lookingForName)
        {
          boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<std::string, ReflectableObject>(_tempStringValue.assign(value, length), _reflectable), reflection_map.map[_currentName]);

          if(ReflectableObject::json_tag_source == _currentName)
          {
            _reflectable.*ReflectableObject::json_tag_target = ReflectableObject::json_tag_map[reflection_map.map[_currentName].*_reflectable];

            boost::apply_visitor(VariantDeserializationVisitor<PrimativeDeserializationVisitor, ObjectDeserializationVisitor, ReflectableObject>(_variantReaderHandler), _reflectable.*ReflectableObject::json_tag_target);
          }
        }
        else
          _currentName.assign(value, length);

        _lookingForName = !_lookingForName;
      }
    }
    virtual std::unique_ptr<BaseReaderHandler> && StartObject(std::unique_ptr<BaseReaderHandler> && result)
    {
      if(_variantReaderHandler != nullptr)
        return std::move(_variantReaderHandler->StartObject(std::move(result)));
      else
      {
        if(!_lookingForName)
          boost::apply_visitor(typename ObjectDeserializationVisitor::template type<ReflectableObject>(result, _reflectable), reflection_map.map[_currentName]);

        _lookingForName = true;
        return std::move(result);
      }
    }
    virtual void EndObject(size_t) { Default(); }
    virtual std::unique_ptr<BaseReaderHandler> && StartArray(std::unique_ptr<BaseReaderHandler> && result)
    {
      if(_variantReaderHandler != nullptr)
        return std::move(_variantReaderHandler->StartArray(std::move(result)));
      else
      {
        if(!_lookingForName)
          boost::apply_visitor(typename DeserializationArrayReflectableVisitor::template type<ReflectableObject>(result, _reflectable), reflection_map.map[_currentName]);

        _lookingForName = true;

        return std::move(result);
      }
    }
    virtual void EndArray(size_t) { Default(); }
  };

  return std::unique_ptr<BaseReaderHandler>(new ReaderHandler(reflectable));
}

template <typename PrimativeDeserializationVisitor, typename ObjectDeserializationVisitor, typename ReflectableObject>
std::unique_ptr<BaseReaderHandler> DeserializeHandler(ReflectableObject& reflectable)
{
  return std::move(DeserializeHandler < PrimativeDeserializationVisitor,
                   ObjectDeserializationVisitor, ReflectableObject > (reflectable, 0));
}

#endif
