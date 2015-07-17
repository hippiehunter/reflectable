#pragma once

#include <map>
#include <type_traits>
#include <vector>
#include <string>
#include <memory>
#include <stack>
#include <utility>
#include <stdexcept>
#include <boost/numeric/conversion/converter.hpp>
#include <boost/variant/static_visitor.hpp>
#include <boost/format.hpp>
#include "rapidjson/rapidjson.h"
#include "rapidjson/reader.h"
#include "rapidjson/writer.h"


#define GENERATE_HAS_MEMBER(member)                                               \
                                                                                  \
template < class T >                                                              \
class HasMember_##member                                                          \
{                                                                                 \
private:                                                                          \
    using Yes = char[2];                                                          \
    using  No = char[1];                                                          \
                                                                                  \
    struct Fallback { int member; };                                              \
    struct Derived : T, Fallback { };                                             \
                                                                                  \
    template < class U >                                                          \
    static No& test ( decltype(U::member)* );                                     \
    template < typename U >                                                       \
    static Yes& test ( U* );                                                      \
                                                                                  \
public:                                                                           \
    static constexpr bool RESULT = sizeof(test<Derived>(nullptr)) == sizeof(Yes); \
};                                                                                \
                                                                                  \
template < class T >                                                              \
struct has_member_##member                                                        \
: public std::integral_constant<bool, HasMember_##member<T>::RESULT>              \
{ };


GENERATE_HAS_MEMBER(reflectable);

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
  virtual void EndArray(size_t) = 0;
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

    template<typename Tn, typename T2>
    auto impl(Tn ReflectableObject::* data, int, typename std::enable_if<std::is_arithmetic<T2>::value>::type* dummy = 0) const -> decltype(std::declval<typename std::remove_pointer<Tn>::type::inflatable>(), void())
    {
      _reflectable.*data = std::remove_pointer<Tn>::type::inflatable::inflate(boost::numeric::converter<typename std::remove_pointer<Tn>::type::inflatable::inflate_t, T2>::convert(_value));
    }

    template<typename Tn, typename T2>
    auto impl(Tn ReflectableObject::* data, int, typename std::enable_if<std::is_arithmetic<Tn>::value>::type* dummy = 0, typename std::enable_if<std::is_arithmetic<T2>::value>::type* dummy2 = 0, typename std::enable_if<!std::is_same<Tn, T2>::value>::type* dummy3 = 0) const -> void
    {
      _reflectable.*data = boost::numeric::converter<Tn, T>::convert(_value);
    }

    template<typename Tn, typename T2>
    auto impl(Tn ReflectableObject::* data, int) const -> typename std::enable_if<std::is_same<Tn, T2>::value, decltype(void())>::type
    {
      _reflectable.*data = _value;
    }

    template<typename Tn, typename T2>
    void impl(Tn ReflectableObject::* data, unsigned int) const
    {
      throw std::runtime_error((boost::format("DeserializationReflectableVisitor: field type %1% doesnt match field type %2% in object type %3%") % typeid(Tn).name() % typeid(T2).name() % typeid(ReflectableObject).name()).str());
    }

    template<typename Tn>
    auto operator()(Tn ReflectableObject::* data) const -> void
    {
      impl<Tn, T>(data, 0);
    }

  };
};

struct ArrayDeserializationReflectableVisitor
{
  template<typename T, typename ReflectableArray>
  class type : public boost::static_visitor<>
  {
    const T& _value;
    ReflectableArray& _reflectable;
  public:
    type(const T& value, ReflectableArray& reflectable) : _value(value), _reflectable(reflectable) {}

    template<typename Tn, typename T2>
    auto impl(int, typename std::enable_if<std::is_arithmetic<T2>::value>::type* dummy = 0) const -> decltype(std::declval<typename std::remove_pointer<Tn>::type::inflatable>(), void())
    {
      _reflectable.push_back(typename std::remove_pointer<Tn>::type::inflatable::inflate(boost::numeric::converter<typename std::remove_pointer<Tn>::type::inflatable::inflate_t, T2>::convert(_value)));
    }

    template<typename Tn, typename T2>
    auto impl(int) const -> typename std::enable_if<boost::type_traits::ice_and<std::is_arithmetic<Tn>::value, std::is_arithmetic<T2>::value>::value, decltype(void())>::type
    {
      _reflectable.push_back(boost::numeric::converter<Tn, T>::convert(_value));
    }

    template<typename Tn, typename T2>
    auto impl2(int) const -> typename std::enable_if<std::is_same<Tn, T2>::value, decltype(void())>::type
    {
      _reflectable.push_back(_value);
    }

    template<typename Tn, typename T2>
    void impl2(char) const
    {
      throw std::runtime_error((boost::format("DeserializationReflectableVisitor: field type %1% doesnt match field type %2% in array type %3%") % typeid(Tn).name() % typeid(T2).name() % typeid(ReflectableArray).name()).str());
    }


    template<typename Tn, typename T2>
    void impl(char) const
    {
      impl2<Tn, T2>(0);
    }

    template<typename Tn>
    auto operator()(Tn ReflectableArray::* data) const -> decltype(std::declval<type<T, ReflectableArray>*>()->template impl<Tn, T>(0))
    {
      impl<Tn, T>(0);
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
    auto impl2(Tn ReflectableObject::* data, unsigned int) const -> decltype(std::declval<typename Tn::reflectable>(), void())
    {
      _value = DeserializeHandler<DeserializationReflectableVisitor, DeserializationObjectReflectableVisitor>(_reflectable.*data);
    }

    template<typename Tn, typename ReflObj>
    auto impl(Tn ReflObj::* data, int) const -> decltype(std::declval<typename ReflObj::reflectable>().json_tag_map, void())
    {
      boost::apply_visitor(ReflectableVariantVisitor(_value), _reflectable.*data);
    }

    template<typename Tn>
    void impl(Tn ReflectableObject::* data, unsigned char) const
    {
      impl2(data, 0);
    }

    template<typename Tn>
    void impl2(Tn ReflectableObject::* data, unsigned char, typename std::enable_if<!has_member_reflectable<Tn>::value>::type* dummy1 = 0) const { }

    template<typename Tn>
    auto operator()(Tn ReflectableObject::* data) const -> decltype(std::declval<typename DeserializationObjectReflectableVisitor::template type<ReflectableObject>*>()->impl(std::declval<Tn ReflectableObject::*>(), 0))
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
    auto impl(Tn ReflectableObject2::* data, int) const -> decltype(std::declval<ReflectableObject2>().push_back(std::declval<Tn>()), void())
    {
      _value = DeserializeArrayHandler<DeserializationReflectableVisitor, DeserializationObjectReflectableVisitor>(_reflectable);
    }
    //we're a nested vector or something like that
    template<typename Tn>
    auto impl(Tn ReflectableObject::* data, int) const -> decltype(std::declval<Tn>().push_back(std::declval<typename Tn::value_type>()), void())
    {
      //(_reflectable.*data).push_back(typename Tn::value_type());
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
  void impl(int ReflectableObject::* data, int) const
  {
    _writer.Int(_reflectable.*data);
  }

  void impl(unsigned ReflectableObject::* data, int) const
  {
    _writer.Uint(_reflectable.*data);
  }

  void impl(int64_t ReflectableObject::* data, int) const
  {
    _writer.Int64(_reflectable.*data);
  }

  void impl(uint64_t ReflectableObject::* data, int) const
  {
    _writer.Uint64(_reflectable.*data);
  }

  void impl(bool ReflectableObject::* data, int) const
  {
    _writer.Bool(_reflectable.*data);
  }

  void impl(double ReflectableObject::* data, int) const
  {
    _writer.Double(_reflectable.*data);
  }

  void impl(std::string ReflectableObject::* data) const
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
  auto impl(Tn ReflectableObject::* data, int) const -> decltype(std::declval<std::remove_pointer<Tn>::type::inflatable>(), void())
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
  auto impl(Tn ReflectableObject::* data, unsigned char) const ->
    decltype(std::declval<typename Tn::types>(), void())
  {
    boost::apply_visitor(VariantSerializationVisitor(_writer), _reflectable.*data);
  }

  template<typename Tn>
  auto operator()(Tn ReflectableObject::* data) const -> decltype((std::declval<SerializationReflectableVisitor<Writer, ReflectableObject>*>())->template impl<Tn>(std::declval<Tn ReflectableObject::*>(), 0))
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
  auto impl(Tn& data, short) const -> decltype(std::declval<typename Tn::reflectable>(), void())
  {
    Serialize(_writer, data);
  }

  template<typename Tn>
  auto impl(Tn& data, unsigned int) const -> decltype(std::declval<std::remove_pointer<Tn>::type::inflatable>(), void())
  {
    operator()(std::remove_pointer<Tn>::type::inflatable::deflate(data));
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
  auto impl(Tn& data, int) const ->
    decltype(std::declval<typename Tn::types>(), void())
  {
    boost::apply_visitor(VariantSerializationVisitor(_writer), data);
  }

  template<typename Tn>
  auto operator()(Tn& data) const -> decltype(std::declval<SerializationArrayReflectableVisitor<Writer>*>()->template impl<Tn>(std::declval<Tn>(), 0))
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

  for (auto&& pair : reflection_map.noLookup)
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

  for (auto&& value : reflectableArray)
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
      this->_handlerStack.push(std::move(handler));
    }

    void Null() { this->_handlerStack.top()->Null(); }
    void Bool(bool value) { this->_handlerStack.top()->Bool(value); }
    void Int(int value) { this->_handlerStack.top()->Int(value); }
    void Uint(unsigned value) { this->_handlerStack.top()->Uint(value); }
    void Int64(int64_t value) { this->_handlerStack.top()->Int64(value); }
    void Uint64(uint64_t value) { this->_handlerStack.top()->Uint64(value); }
    void Double(double value) { this->_handlerStack.top()->Double(value); }
    void String(const char* value, size_t length, bool b)
    {
      auto&& topHandler = this->_handlerStack.top();
      auto ptr = topHandler.get();
      ptr->String(value, length, b);
    }
    void StartObject()
    {
      auto result = std::move(this->_handlerStack.top()->StartObject(std::unique_ptr<BaseReaderHandler>(nullptr)));

      if (result)
        this->_handlerStack.push(std::move(result));
    }
    void EndObject(size_t size)
    {
      if (this->_handlerStack.top())
      {
        this->_handlerStack.top()->EndObject(size);
        this->_handlerStack.pop();
      }
      else
        throw std::runtime_error("mismatched object");
    }
    void StartArray()
    {
      auto result = std::move(this->_handlerStack.top()->StartArray(std::unique_ptr<BaseReaderHandler>(nullptr)));
      if (result)
        this->_handlerStack.push(std::move(result));
    }
    void EndArray(size_t size)
    {
      if (this->_handlerStack.top())
      {
        this->_handlerStack.top()->EndArray(size);
        this->_handlerStack.pop();
      }
      else
        throw std::runtime_error("mismatched array");
    }
  };
  rapidjson::Reader reader;
  ReaderHandler rHandler(std::move(handler));
  reader.Parse<0>(stream, rHandler);
}

template <typename PrimativeDeserializationVisitor, typename ObjectDeserializationVisitor, typename ReflectableArray>
struct DeserializeArrayHandlerReaderHandler : public BaseReaderHandler
{
  DeserializeArrayHandlerReaderHandler(ReflectableArray& reflectable) : _reflectable(reflectable) { }
  ReflectableArray& _reflectable;
  std::string _tempStringValue;

  void Default() {}
  virtual void Null() {  }
  virtual void Bool(bool value)
  {
    typename ArrayDeserializationReflectableVisitor::template type<bool, ReflectableArray>(value, _reflectable)(static_cast<typename ReflectableArray::value_type ReflectableArray::*>(nullptr));
  }
  virtual void Int(int value)
  {
    typename ArrayDeserializationReflectableVisitor::template type<int, ReflectableArray>(value, _reflectable)(static_cast<typename ReflectableArray::value_type ReflectableArray::*>(nullptr));
  }
  virtual void Uint(unsigned value)
  {
    typename ArrayDeserializationReflectableVisitor::template type<unsigned, ReflectableArray>(value, _reflectable)(static_cast<typename ReflectableArray::value_type ReflectableArray::*>(nullptr));
  }
  virtual void Int64(int64_t value)
  {
    typename ArrayDeserializationReflectableVisitor::template type<int64_t, ReflectableArray>(value, _reflectable)(static_cast<typename ReflectableArray::value_type ReflectableArray::*>(nullptr));
  }
  virtual void Uint64(uint64_t value)
  {
    typename ArrayDeserializationReflectableVisitor::template type<uint64_t, ReflectableArray>(value, _reflectable)(static_cast<typename ReflectableArray::value_type ReflectableArray::*>(nullptr));
  }
  virtual void Double(double value)
  {
    typename ArrayDeserializationReflectableVisitor::template type<double, ReflectableArray>(value, _reflectable)(static_cast<typename ReflectableArray::value_type ReflectableArray::*>(nullptr));
  }
  virtual void String(const char* value, size_t length, bool)
  {
    typename ArrayDeserializationReflectableVisitor::template type<std::string, ReflectableArray>(_tempStringValue.assign(value, length), _reflectable)(static_cast<typename ReflectableArray::value_type ReflectableArray::*>(nullptr));
  }

  template<typename ReflArray>
  auto StartObjectImpl(std::unique_ptr<BaseReaderHandler> && result, int) -> decltype(std::declval<typename ReflArray::value_type::reflectable>(), std::move(std::declval<std::unique_ptr<BaseReaderHandler>>()))
  {
    _reflectable.push_back(typename ReflArray::value_type());
    result = std::move(DeserializeHandler<PrimativeDeserializationVisitor, ObjectDeserializationVisitor, typename ReflArray::value_type>(_reflectable.back()));
    return std::move(result);

  }
  template<typename ReflArray>
  auto StartObjectImpl(std::unique_ptr<BaseReaderHandler> && result, long) -> decltype(std::move(std::declval<std::unique_ptr<BaseReaderHandler>>()))
  {
    throw std::runtime_error((boost::format("DeserializeArrayHandlerReaderHandler: failed to start object")).str());
    return std::move(result);
  }


  virtual std::unique_ptr<BaseReaderHandler> && StartObject(std::unique_ptr<BaseReaderHandler> && result)
  {
    result = std::move(StartObjectImpl<ReflectableArray>(std::move(result), 0));
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

template <typename PrimativeDeserializationVisitor, typename ObjectDeserializationVisitor, typename ReflectableArray>
std::unique_ptr<BaseReaderHandler> DeserializeArrayHandler(ReflectableArray& reflectable)
{
  return std::unique_ptr<BaseReaderHandler>(new DeserializeArrayHandlerReaderHandler<PrimativeDeserializationVisitor, ObjectDeserializationVisitor, ReflectableArray>(reflectable));
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

    auto current_field() -> decltype(std::declval<typename ReflectableObject::reflectable>().map[""])
    {
      auto reflectionEntry = _reflection_map.map.find(_currentName);
      if (reflectionEntry == _reflection_map.map.end())
        throw std::runtime_error((boost::format("field %1% not found in type %2%") % _currentName % typeid(ReflectableObject).name()).str());
      else
        return reflectionEntry->second;
    }

    void Default() {}
    virtual void Null() { _lookingForName = true; }
    virtual void Bool(bool value)
    {
      /*if(_inArray)
        typename PrimativeDeserializationVisitor::type<bool, ReflectableObject>(value, _reflectable)(static_cast<bool ReflectableObject::*>(nullptr));
      else*/
      boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<bool, ReflectableObject>(value, _reflectable), current_field());
      _lookingForName = true;
    }
    virtual void Int(int value)
    {
      boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<int, ReflectableObject>(value, _reflectable), current_field());
      _lookingForName = true;
    }
    virtual void Uint(unsigned value)
    {
      boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<unsigned, ReflectableObject>(value, _reflectable), current_field());
      _lookingForName = true;
    }
    virtual void Int64(int64_t value)
    {
      boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<int64_t, ReflectableObject>(value, _reflectable), current_field());
      _lookingForName = true;
    }
    virtual void Uint64(uint64_t value)
    {
      boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<uint64_t, ReflectableObject>(value, _reflectable), current_field());
      _lookingForName = true;
    }
    virtual void Double(double value)
    {
      boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<double, ReflectableObject>(value, _reflectable), current_field());
      _lookingForName = true;
    }
    virtual void String(const char* value, size_t length, bool)
    {
      if (!_lookingForName)
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<std::string, ReflectableObject>(_tempStringValue.assign(value, length), _reflectable), current_field());

      else
        _currentName.assign(value, length);

      _lookingForName = !_lookingForName;

    }
    virtual std::unique_ptr<BaseReaderHandler> && StartObject(std::unique_ptr<BaseReaderHandler> && result)
    {
      if (!_lookingForName)
        boost::apply_visitor(typename ObjectDeserializationVisitor::template type<ReflectableObject>(result, _reflectable), current_field());

      _lookingForName = true;

      return std::move(result);
    }
    virtual void EndObject(size_t) { Default(); }
    virtual std::unique_ptr<BaseReaderHandler> && StartArray(std::unique_ptr<BaseReaderHandler> && result)
    {
      if (!_lookingForName)
        boost::apply_visitor(typename DeserializationArrayReflectableVisitor::template type<ReflectableObject>(result, _reflectable), current_field());

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
auto DeserializeHandler(ReflectableObject& reflectable, int) -> decltype(std::declval<typename ReflectableObject::reflectable>().json_tag_map, std::unique_ptr<BaseReaderHandler>())
{
  static typename ReflectableObject::reflectable reflection_map;
  struct ReaderHandler : public BaseReaderHandler
  {
    ReaderHandler(ReflectableObject& reflectable, typename ReflectableObject::reflectable& reflection_map) : _reflectable(reflectable), _reflection_map(reflection_map), _lookingForName(true), _skipNextFieldName(false) { }
    ReflectableObject& _reflectable;

    bool _lookingForName;
    bool _skipNextFieldName;
    std::string _currentName;
    std::string _tempStringValue;
    std::unique_ptr<BaseReaderHandler> _variantReaderHandler;
    typename ReflectableObject::reflectable& _reflection_map;

    auto current_field() -> decltype(std::declval<typename ReflectableObject::reflectable>().map[""])
    {
      auto reflectionEntry = _reflection_map.map.find(_currentName);
      if (reflectionEntry == _reflection_map.map.end())
        throw std::runtime_error((boost::format("DeserializeHandler: field %1% not found in type %2%") % _currentName % typeid(ReflectableObject).name()).str());
      else
        return reflectionEntry->second;
    }

    void Default() {}
    virtual void Null() { _lookingForName = true; }
    virtual void Bool(bool value)
    {
      if (_variantReaderHandler != nullptr)
        _variantReaderHandler->Bool(value);
      else
      {
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<bool, ReflectableObject>(value, _reflectable), current_field());
        _lookingForName = true;
      }
    }
    virtual void Int(int value)
    {
      if (_variantReaderHandler != nullptr)
        _variantReaderHandler->Int(value);
      else
      {
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<int, ReflectableObject>(value, _reflectable), current_field());
        _lookingForName = true;
      }
    }
    virtual void Uint(unsigned value)
    {
      if (_variantReaderHandler != nullptr)
        _variantReaderHandler->Uint(value);
      else
      {
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<int, ReflectableObject>(value, _reflectable), current_field());
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<unsigned, ReflectableObject>(value, _reflectable), current_field());
        _lookingForName = true;
      }
    }
    virtual void Int64(int64_t value)
    {
      if (_variantReaderHandler != nullptr)
        _variantReaderHandler->Int64(value);
      else
      {
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<int64_t, ReflectableObject>(value, _reflectable), current_field());
        _lookingForName = true;
      }
    }
    virtual void Uint64(uint64_t value)
    {
      if (_variantReaderHandler != nullptr)
        _variantReaderHandler->Uint64(value);
      else
      {
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<int64_t, ReflectableObject>(value, _reflectable), current_field());
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<uint64_t, ReflectableObject>(value, _reflectable), current_field());
        _lookingForName = true;
      }
    }
    virtual void Double(double value)
    {
      if (_variantReaderHandler != nullptr)
        _variantReaderHandler->Double(value);
      else
      {
        boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<double, ReflectableObject>(value, _reflectable), current_field());
        _lookingForName = true;
      }
    }

    virtual void String(const char* value, size_t length, bool unnamedBool)
    {
      typedef typename decltype(_reflection_map.json_tag_map)::key_type tag_map_key_t;


      if (_skipNextFieldName)
      {
        _skipNextFieldName = false;
      }
      else if (_variantReaderHandler != nullptr)
        _variantReaderHandler->String(value, length, unnamedBool);
      else
      {
        if (!_lookingForName)
        {
          boost::apply_visitor(typename PrimativeDeserializationVisitor::template type<std::string, ReflectableObject>(_tempStringValue.assign(value, length), _reflectable), current_field());

          if (_reflection_map.json_tag_source == _currentName)
          {
            _skipNextFieldName = true;
            _reflectable.*_reflection_map.json_tag_target = _reflection_map.json_tag_map[_reflectable.*boost::get<tag_map_key_t ReflectableObject::*>(current_field())];

            boost::apply_visitor(VariantDeserializationVisitor<PrimativeDeserializationVisitor, ObjectDeserializationVisitor, ReflectableObject>(_variantReaderHandler), _reflectable.*_reflection_map.json_tag_target);
          }
        }
        else
          _currentName.assign(value, length);

        _lookingForName = !_lookingForName;
      }
    }
    virtual std::unique_ptr<BaseReaderHandler> && StartObject(std::unique_ptr<BaseReaderHandler> && result)
    {
      if (_variantReaderHandler != nullptr)
      {
        _variantReaderHandler->StartObject(std::move(std::unique_ptr<BaseReaderHandler>(nullptr)));
        result = std::move(_variantReaderHandler);
        _variantReaderHandler = nullptr;
        return std::move(result);
      }
      else
      {
        if (!_lookingForName)
          boost::apply_visitor(typename ObjectDeserializationVisitor::template type<ReflectableObject>(result, _reflectable), current_field());

        _lookingForName = true;
        return std::move(result);
      }
    }
    virtual void EndObject(size_t)
    {
      //_variantReaderHandler = nullptr;
      Default();
    }
    virtual std::unique_ptr<BaseReaderHandler> && StartArray(std::unique_ptr<BaseReaderHandler> && result)
    {
      if (_variantReaderHandler != nullptr)
        return std::move(_variantReaderHandler->StartArray(std::move(result)));
      else
      {
        if (!_lookingForName)
        {
          boost::apply_visitor(typename DeserializationArrayReflectableVisitor::template type<ReflectableObject>(result, _reflectable), current_field());
        }

        _lookingForName = true;

        return std::move(result);
      }
    }
    virtual void EndArray(size_t) { Default(); }
  };

  return std::unique_ptr<BaseReaderHandler>(new ReaderHandler(reflectable, reflection_map));
}

template <typename PrimativeDeserializationVisitor, typename ObjectDeserializationVisitor, typename ReflectableObject>
std::unique_ptr<BaseReaderHandler> DeserializeHandler(ReflectableObject& reflectable)
{
  return DeserializeHandler < PrimativeDeserializationVisitor,
    ObjectDeserializationVisitor, ReflectableObject >(reflectable, 0);
}
