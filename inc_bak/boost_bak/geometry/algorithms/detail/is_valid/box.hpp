// Boost.Geometry (aka GGL, Generic Geometry Library)

// Copyright (c) 2014, Oracle and/or its affiliates.

// Contributed and/or modified by Menelaos Karavelas, on behalf of Oracle

// Licensed under the Boost Software License version 1.0.
// http://www.boost.org/users/license.html

#ifndef BOOST_GEOMETRY_ALGORITHMS_DETAIL_IS_VALID_BOX_HPP
#define BOOST_GEOMETRY_ALGORITHMS_DETAIL_IS_VALID_BOX_HPP

#include <cstddef>

#include <boost/geometry/core/access.hpp>
#include <boost/geometry/core/tags.hpp>
#include <boost/geometry/core/coordinate_dimension.hpp>

#include <boost/geometry/algorithms/dispatch/is_valid.hpp>


namespace boost { namespace geometry
{


#ifndef DOXYGEN_NO_DETAIL
namespace detail { namespace is_valid
{

template <typename Box, std::size_t I>
struct has_valid_corners
{
    static inline bool apply(Box const& box)
    {
        if ( geometry::get<geometry::max_corner, I-1>(box)
             <=
             geometry::get<geometry::min_corner, I-1>(box) )
        {
            return false;
        }
        return has_valid_corners<Box, I-1>::apply(box);
    }
};


template <typename Box>
struct has_valid_corners<Box, 0>
{
    static inline bool apply(Box const&)
    {
        return true;
    }
};

}} // namespace detail::is_valid
#endif // DOXYGEN_NO_DETAIL



#ifndef DOXYGEN_NO_DISPATCH
namespace dispatch
{


// A box is always simple
// A box is a Polygon, and it satisfies the conditions for Polygon validity.
//
// The only thing we have to check is whether the max corner lies in
// the upper-right quadrant as defined by the min corner
//
// Reference (for polygon validity): OGC 06-103r4 (6.1.11.1)
template <typename Box>
struct is_valid<Box, box_tag>
    : detail::is_valid::has_valid_corners<Box, dimension<Box>::value>
{};


} // namespace dispatch
#endif // DOXYGEN_NO_DISPATCH



}} // namespace boost::geometry


#endif // BOOST_GEOMETRY_ALGORITHMS_DETAIL_IS_VALID_BOX_HPP
