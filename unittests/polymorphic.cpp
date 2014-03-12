#include "common.hpp"

struct PolyBase
{
  PolyBase() {}
  PolyBase( int xx, float yy ) : x(xx), y(yy) {}
  int x;
  float y;

  template <class Archive>
  void serialize( Archive & ar )
  {
    ar( x, y );
  }

  virtual void foo() = 0;

  bool operator==( PolyBase const & other ) const
  {
    return x == other.x && std::abs(y - other.y) < 1e-5;
  }
};

struct PolyDerived : PolyBase
{
  PolyDerived() {}
  PolyDerived( int xx, float yy, bool aa, double bb ) :
    PolyBase( xx, yy ), a(aa), b(bb) {}

  bool a;
  double b;

  template <class Archive>
  void serialize( Archive & ar )
  {
    ar( cereal::base_class<PolyBase>( this ),
        a, b );
  }

  bool operator==( PolyDerived const & other ) const
  {
    return PolyBase::operator==( other ) && a == other.a && std::abs(b - other.b) < 1e-5;
  }

  void foo() {}
};

struct PolyLA : std::enable_shared_from_this<PolyLA>
{
  virtual void foo() = 0;
};

struct PolyDerivedLA : public PolyLA
{
  PolyDerivedLA( int xx ) : x( xx ) { }

  int x;

  template <class Archive>
  void serialize( Archive & ar )
  {
    ar( x );
  }

  template <class Archive>
  static void load_and_construct( Archive & ar, cereal::construct<PolyDerivedLA> & construct )
  {
    int xx;
    ar( xx );
    construct( xx );
  }

  void foo() {}

  bool operator==( PolyDerivedLA const & other ) const
  {
    return x == other.x;
  }
};

std::ostream& operator<<(std::ostream& os, PolyDerivedLA const & s)
{
    os << "[x: " << s.x << "]";
    return os;
}

std::ostream& operator<<(std::ostream& os, PolyDerived const & s)
{
    os << "[x: " << s.x << " y: " << s.y << " a: " << s.a << " b: " << s.b << "]";
    return os;
}

CEREAL_REGISTER_TYPE(PolyDerived)
CEREAL_REGISTER_TYPE(PolyDerivedLA)

template <class IArchive, class OArchive>
void test_polymorphic()
{
  std::random_device rd;
  std::mt19937 gen(rd());

  auto rngB = [&](){ return random_value<int>( gen ) % 2 == 0; };
  auto rngI = [&](){ return random_value<int>( gen ); };
  auto rngF = [&](){ return random_value<float>( gen ); };
  auto rngD = [&](){ return random_value<double>( gen ); };

  for(int ii=0; ii<100; ++ii)
  {
    std::shared_ptr<PolyBase> o_shared = std::make_shared<PolyDerived>( rngI(), rngF(), rngB(), rngD() );
    std::weak_ptr<PolyBase>   o_weak = o_shared;
    std::unique_ptr<PolyBase> o_unique( new PolyDerived( rngI(), rngF(), rngB(), rngD() ) );
    std::shared_ptr<PolyLA>   o_sharedLA = std::make_shared<PolyDerivedLA>( rngI() );

    std::ostringstream os;
    {
      OArchive oar(os);

      oar( o_shared, o_weak, o_unique );
      oar( o_sharedLA );
    }

    decltype(o_shared) i_shared;
    decltype(o_weak) i_weak;
    decltype(o_unique) i_unique;
    decltype(o_sharedLA) i_sharedLA;

    std::istringstream is(os.str());
    {
      IArchive iar(is);

      iar( i_shared, i_weak, i_unique );
      iar( i_sharedLA );
    }

    auto i_locked = i_weak.lock();
    auto o_locked = o_weak.lock();

    auto i_sharedLA2 = i_sharedLA->shared_from_this();

    BOOST_CHECK_EQUAL(i_shared.get(), i_locked.get());
    BOOST_CHECK_EQUAL(*((PolyDerived*)i_shared.get()), *((PolyDerived*)o_shared.get()));
    BOOST_CHECK_EQUAL(*((PolyDerived*)i_shared.get()), *((PolyDerived*)i_locked.get()));
    BOOST_CHECK_EQUAL(*((PolyDerived*)i_locked.get()), *((PolyDerived*)o_locked.get()));
    BOOST_CHECK_EQUAL(*((PolyDerived*)i_unique.get()), *((PolyDerived*)o_unique.get()));
    BOOST_CHECK_EQUAL(*((PolyDerivedLA*)i_sharedLA.get()), *((PolyDerivedLA*)o_sharedLA.get()));
    BOOST_CHECK_EQUAL(*((PolyDerivedLA*)i_sharedLA2.get()), *((PolyDerivedLA*)o_sharedLA.get()));
  }
}

BOOST_AUTO_TEST_CASE( binary_polymorphic )
{
  test_polymorphic<cereal::BinaryInputArchive, cereal::BinaryOutputArchive>();
}

BOOST_AUTO_TEST_CASE( portable_binary_polymorphic )
{
  test_polymorphic<cereal::PortableBinaryInputArchive, cereal::PortableBinaryOutputArchive>();
}

BOOST_AUTO_TEST_CASE( xml_polymorphic )
{
  test_polymorphic<cereal::XMLInputArchive, cereal::XMLOutputArchive>();
}

BOOST_AUTO_TEST_CASE( json_polymorphic )
{
  test_polymorphic<cereal::JSONInputArchive, cereal::JSONOutputArchive>();
}

