#include <reflectable/reflectable_impl.h>
#include <json/json_serialization.h>

#include <json/json_serialization.h>
#include <json/rapidjson/writer.h>
#include <json/rapidjson/reader.h>
#include <json/rapidjson/filestream.h>

#include <string>
#include <vector>
#include <iostream>



struct thing
{
  struct link
  {
    ENABLE_REFLECTION
    std::string _name;
    std::string subreddit;
    std::string title;
  };
  
  struct listing
  {
    ENABLE_REFLECTION
    std::string before;
    std::string after;
    std::vector<thing> children;
  };
  
  ENABLE_REFLECTION
  std::string _id;
  std::string kind;
  boost::variant<link, listing> data;
};

REFLECTABLE(
  (thing)
  ((std::map<std::string, decltype(thing::data)> json_tag_map = {{"t1", link()}, {"listing", listing()}}))
  ((decltype(thing::data) thing::* json_tag_target = &thing::data))
  ((std::string json_tag_source = "kind"))
  ((struct json_tagged {})),
  (std::string, _id),
  (std::string, kind),
  (decltype(thing::data), data))

REFLECTABLE(
  (thing::link)(),
  (std::string, _name),
  (std::string, subreddit),
  (std::string, title))


REFLECTABLE(
  (thing::listing)(),
  (std::string, before),
  (std::string, after),
  (std::vector<thing>, children))

struct inflatableCrap
{
  int theValue;
  struct inflatable
  {
  public:
    static int deflate(const inflatableCrap& crap) { return crap.theValue; }
    static inflatableCrap inflate(int theValue) { return inflatableCrap { theValue }; }
  };
};

struct somecrap
{
  ENABLE_REFLECTION
  inflatableCrap myInflator;
};

struct hasVector
{
  ENABLE_REFLECTION
  std::vector<int> myVec;
  std::vector<std::string> myStringVec;
  std::vector<inflatableCrap> myInflatableVector;
  std::vector<bool> myBoolVector;
  std::vector<float> myFloatVector;
  std::vector<double> myDoubleVector;
  std::vector<short> myShortVector;
};

REFLECTABLE(
  (hasVector)(),
  (std::vector<int>, myVec),
  (std::vector<std::string>, myStringVec),
  (std::vector<inflatableCrap>, myInflatableVector),
  (std::vector<bool>, myBoolVector),
  (std::vector<float>, myFloatVector),
  (std::vector<double>, myDoubleVector),
  (std::vector<short>, myShortVector))

REFLECTABLE(
  (somecrap)(),
  (inflatableCrap, myInflator))


int main(int argc, char **argv) 
{
  std::string snarf("{\"myInflator\":5}");
    std::string bla("{\"id\":\"\",\"kind\":\"t1\",\"data\":{\"name\":\"t1_asdfg\",\"subreddit\":\"programming\",\"title\":\"some link\"}}");
    thing th;
    thing th2;
    somecrap crp;
    somecrap crp2;
    hasVector hv;
    crp.myInflator = inflatableCrap { 9 };
    th.kind = "t1";
	thing::link lnk = { "t1_asdfg", "programming", "some link" };
    th.data = lnk; 
    //rapidjson::FileStream s(std::cout);
    //rapidjson::Writer<rapidjson::FileStream> writer(s);
    //Serialize(writer, th);
    //Serialize(writer, crp);
    rapidjson::GenericStringStream<rapidjson::UTF8<>> inStream("{\"id\":\"\",\"kind\":\"t1\",\"data\":{\"name\":\"t1_asdfgg\",\"subreddit\":\"programming\",\"title\":\"some link\"}}");
    Deserialize(inStream, std::move(DeserializeHandler<DeserializationReflectableVisitor, DeserializationObjectReflectableVisitor>(th)));
    
    
    rapidjson::GenericStringStream<rapidjson::UTF8<>> inStream2("{\"myInflator\":5}");
    Deserialize(inStream2, std::move(DeserializeHandler<DeserializationReflectableVisitor, DeserializationObjectReflectableVisitor>(crp2)));
    
    rapidjson::GenericStringStream<rapidjson::UTF8<>> inStream3("{\"myVec\":[5,6,7],\"myStringVec\":[\"strings\", \"are\", \"working\"], \"myInflatableVector\":[1,2,3], \"myBoolVector\":[true,false], \"myFloatVector\":[1.0, 1.5, 1.337], \"myDoubleVector\":[1.0, 1.5, 1.337], \"myShortVector\":[5,6,7]}");
    Deserialize(inStream3, std::move(DeserializeHandler<DeserializationReflectableVisitor, DeserializationObjectReflectableVisitor>(hv)));
    
    rapidjson::GenericStringStream<rapidjson::UTF8<>> inStream4("{\"id\":\"\",\"kind\":\"listing\",\"data\":{\"before\":\"t1_asdfgg\",\"after\":\"programming\",\"children\":[{\"id\":\"\",\"kind\":\"t1\",\"data\":{\"name\":\"t1_asdfgg\",\"subreddit\":\"programming\",\"title\":\"some link\"}}]}}");
    Deserialize(inStream4, std::move(DeserializeHandler<DeserializationReflectableVisitor, DeserializationObjectReflectableVisitor>(th2)));
    
    
    std::cout << hv.myVec.size() << std::endl;
    //Serialize(writer, crp2);
    //Serialize(writer, th);
    
    std::cout << "Hello, world!" << std::endl;
    return 0;
}
