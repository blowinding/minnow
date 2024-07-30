#include "wrapping_integers.hh"
#include <iostream>
using namespace std;

Wrap32 Wrap32::wrap( uint64_t n, Wrap32 zero_point )
{
  return Wrap32 { static_cast<uint32_t>( ( zero_point.raw_value_ + n ) ) };
}

uint64_t Wrap32::unwrap( Wrap32 zero_point, uint64_t checkpoint ) const
{
  uint32_t distance = raw_value_ - zero_point.raw_value_;
  auto point = ( checkpoint + ( 1UL << 31 ) ) / ( 1UL << 32 ) * ( 1UL << 32 );
  auto judge = static_cast<long long>( point + distance - checkpoint );
  if ( abs( judge ) <= 1LL << 31 || point == 0 )
    point += distance;
  else
    point -= ( ( 1UL << 32 ) - distance );
  return point;
}
