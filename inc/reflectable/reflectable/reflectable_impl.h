#ifndef REFLECTABLE_IMPL_H
#define REFLECTABLE_IMPL_H

//this must be valid on your compiler/platform of choice or we cant do our macro magic
#define BOOST_PP_VARIADICS 1
#define DEFINE_REFLECTION

#include <boost/preprocessor.hpp>
#include <boost/mpl/for_each.hpp>
#include <boost/mpl/range_c.hpp>
#include <boost/mpl/transform.hpp>
#include <boost/variant.hpp>
#include <boost/mpl/fold.hpp>
#include <boost/mpl/insert.hpp>
#include <boost/mpl/front_inserter.hpp>
#include <boost/mpl/set.hpp>
#include <boost/mpl/vector.hpp>
#include <boost/type_traits.hpp>
#include <boost/mpl/copy.hpp>
#include <boost/mpl/not.hpp>
#include <boost/mpl/same_as.hpp>
#include <boost/utility/enable_if.hpp>

#include <boost/assign/list_of.hpp>

#include <map>
#include <vector>
#include <string>
#include <memory>
#include <stack>

#include "reflectable.h"

#define TYPEOF(x) BOOST_PP_TUPLE_ELEM(0, x)


#define REM(...) __VA_ARGS__
#define EAT(...)

// Strip off the type
#define STRIP(x) BOOST_PP_TUPLE_ELEM(1, x)


// A helper metafunction for adding const to a type
template<class M, class T>
struct make_const
{
    typedef T type;
};

template<class M, class T>
struct make_const<const M, T>
{
    typedef typename boost::add_const<T>::type type;
};

constexpr const char* strip_underscore(const char* str)
{
  return str[0] == '_' ? str + 1 : str;
}

#define CAT(x, y) CAT_I(x, y) 
#define CAT_I(x, y) x ## y 

#define APPLY(macro, args) APPLY_I(macro, args) 
#define APPLY_I(macro, args) macro args 

#define STRIP_PARENS(x) EVAL((STRIP_PARENS_I x), x) 
#define STRIP_PARENS_I(...) 1,1 

#define EVAL(test, x) EVAL_I(test, x) 
#define EVAL_I(test, x) MAYBE_STRIP_PARENS(TEST_ARITY test, x) 

#define TEST_ARITY(...) APPLY(TEST_ARITY_I, (__VA_ARGS__, 2, 1)) 
#define TEST_ARITY_I(a,b,c,...) c 

#define MAYBE_STRIP_PARENS(cond, x) MAYBE_STRIP_PARENS_I(cond, x) 
#define MAYBE_STRIP_PARENS_I(cond, x) CAT(MAYBE_STRIP_PARENS_, cond)(x) 

#define MAYBE_STRIP_PARENS_1(x) x 
#define MAYBE_STRIP_PARENS_2(x) APPLY(MAYBE_STRIP_PARENS_2_I, x) 
#define MAYBE_STRIP_PARENS_2_I(...) __VA_ARGS__ 

#define REFLECT_EACH_2(r, data, i, x) BOOST_PP_COMMA_IF(i) field_data<i, data>

#define REFLECT_EACH_3(r, data, i, x) BOOST_PP_COMMA_IF(i) decltype(&data::STRIP(x))

#define REFLECT_EACH_4(r, data, i, x) noLookup.push_back(std::make_pair(strip_underscore(BOOST_PP_STRINGIZE(STRIP(x))), data::reflectable::field_type_variant_t(&data::STRIP(x))));
#define REFLECT_EACH_4a(r, data, i, x) map[strip_underscore(BOOST_PP_STRINGIZE(STRIP(x)))] = data::reflectable::field_type_variant_t(&data::STRIP(x)); 

#define REFLECT_EACH_5(r, data, i, x) \
	BOOST_PP_EXPR_IF(BOOST_PP_GREATER(2, BOOST_PP_TUPLE_SIZE(x)),\
	BOOST_PP_SEQ_FOR_EACH_I(REFLECT_EACH_6, (STRIP(X), data), BOOST_PP_TUPLE_TO_SEQ(BOOST_PP_TUPLE_EAT(2)x)))
	
#define REFLECT_EACH_6(r, data, i, x) BOOST_PP_SEQ_HEAD(x); 

#define REFLECT_EACH_ATTRIBUTE_DEF(r, data, x) STRIP_PARENS(x);

#define REFLECT_EACH(r, data, i, x) \
template<class Self> \
struct field_data<i, Self> \
{ \
    auto get() -> decltype(&Self::STRIP(x)) \
    { \
        return &Self::STRIP(x); \
    }\
    const char * name() const \
    {\
        return strip_underscore(BOOST_PP_STRINGIZE(STRIP(x))); \
    } \
};


#define REFLECTABLE(this_lst, ...) struct BOOST_PP_SEQ_HEAD(this_lst)::reflectable { \
	static const int fields_n = BOOST_PP_VARIADIC_SIZE(__VA_ARGS__); \
	friend struct reflector; \
	template<int N, class Self> \
	struct field_data {}; \
	BOOST_PP_SEQ_FOR_EACH_I(REFLECT_EACH, data, BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \
	typedef boost::mpl::vector<BOOST_PP_SEQ_FOR_EACH_I(REFLECT_EACH_2, BOOST_PP_SEQ_HEAD(this_lst), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))> field_data_seq; \
	/*field types*/ \
	typedef boost::mpl::fold<boost::mpl::vector<BOOST_PP_SEQ_FOR_EACH_I(REFLECT_EACH_3, BOOST_PP_SEQ_HEAD(this_lst), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))>, boost::mpl::set0<>, boost::mpl::insert<boost::mpl::_1,boost::mpl::_2>>::type unique_field_types_t; \
	typedef boost::mpl::copy<unique_field_types_t, boost::mpl::front_inserter<boost::mpl::list<>>>::type unique_field_types_list_t;\
	typedef boost::make_variant_over<unique_field_types_list_t>::type field_type_variant_t; \
	/*combine the field values and field types into a map*/ \
	std::map<std::string, BOOST_PP_SEQ_HEAD(this_lst)::reflectable::field_type_variant_t> map; \
	std::vector<std::pair<std::string, BOOST_PP_SEQ_HEAD(this_lst)::reflectable::field_type_variant_t>> noLookup; \
	public: \
	BOOST_PP_SEQ_FOR_EACH(REFLECT_EACH_ATTRIBUTE_DEF, BOOST_PP_SEQ_HEAD(this_lst), BOOST_PP_SEQ_TAIL(this_lst))\
	reflectable() \
	{ \
		BOOST_PP_SEQ_FOR_EACH_I(REFLECT_EACH_4, BOOST_PP_SEQ_HEAD(this_lst), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))\
		BOOST_PP_SEQ_FOR_EACH_I(REFLECT_EACH_4a, BOOST_PP_SEQ_HEAD(this_lst), BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__)) \
		BOOST_PP_SEQ_FOR_EACH_I(REFLECT_EACH_5, BOOST_PP_SEQ_HEAD(this_lst),  BOOST_PP_VARIADIC_TO_SEQ(__VA_ARGS__))\
	}};
	
//
	



struct reflector
{
    //Get field_data at index N
    template<int N, class T>
    static typename T::reflectable::template field_data<N, T> get_field_data()
    {
        return typename T::reflectable::template field_data<N, T>();
    }

    // Get the number of fields
    template<class T>
    struct fields
    {
        static const int n = T::reflectable::fields_n;
    };

	template<int N, class T>
	struct fields_type
	{
		typename T::reflectable::template field_data<N, T> type;
	};
};

template<class C, class Action>
struct field_visitor
{
    Action a;
    C & c;
    field_visitor(Action a, C& c) : a(a), c(c)
    {
    }

    template<class T>
    void operator()(T)
    {
        a(reflector::get_field_data<T::value, C>(), c);
    }
};

template<class C, class Action>
void visit_each(C & c, Action a)
{
    typedef boost::mpl::range_c<int,0,reflector::fields<C>::n> range;
    field_visitor<C, Action> visitor(a, c);
    boost::mpl::for_each<range>(visitor);
}

#endif
