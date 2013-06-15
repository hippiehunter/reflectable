#include <reflectable\reflection_impl.h>
#include <json\json_serialization.h>

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
    std::string name;
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
  std::string id;
  std::string kind;
  boost::variant<link, listing> data;
};

REFLECTABLE(
  (thing)
  ((std::map<std::string, decltype(thing::data)> json_tag_map))
  ((decltype(thing::data) thing::* json_tag_target))
  ((std::string json_tag_source))
  ((struct json_tagged {})),
  (std::string, id),
  (std::string, kind, (json_tag_map = {{"t1", link()}, {"listing", listing()}};json_tag_source = "kind")),
  (std::vector<thing>, data, (json_tag_target = &thing::data)))

REFLECTABLE(
  (thing::link)(),
  (std::string, name),
  (std::string, subreddit),
  (std::string, title))


REFLECTABLE(
  (thing::listing)(),
  (std::string, before),
  (std::string, after),
  (std::vector<thing>, children))


int main(int argc, char **argv) 
{
    std::string bla("{\"id\":\"\",\"kind\":\"t1\",\"data\":{\"name\":\"t1_asdfg\",\"subreddit\":\"programming\",\"title\":\"some link\"}}");
    thing th;
    th.kind = "t1";
	thing::link lnk = { "t1_asdfg", "programming", "some link" };
    th.data = lnk; 
    rapidjson::FileStream s(stdout);
    rapidjson::Writer<rapidjson::FileStream> writer(s);
    Serialize(writer, th);
    rapidjson::GenericStringStream<rapidjson::UTF8<>> inStream("{\"id\":\"\",\"kind\":\"t1\",\"data\":{\"name\":\"t1_asdfg\",\"subreddit\":\"programming\",\"title\":\"some link\"}}");
    Deserialize(inStream, std::move(DeserializeHandler<DeserializationReflectableVisitor, DeserializationObjectReflectableVisitor>(th)));
    std::cout << "Hello, world!" << std::endl;
    return 0;
}
