/* -*-c++-*- */
/* osgEarth - Dynamic map generation toolkit for OpenSceneGraph
 * Copyright 2008-2010 Pelican Mapping
 * http://osgearth.org
 *
 * osgEarth is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>
 */
#include <osgEarthFeatures/FeatureCompiler>
#include <osgEarthFeatures/TransformFilter>
#include <osgEarthFeatures/SubstituteModelFilter>
#include <osgEarthFeatures/BuildGeometryFilter>
#include <osg/MatrixTransform>

#define LC "[FeatureCompiler] "

using namespace osgEarth;
using namespace osgEarth::Features;
using namespace osgEarth::Symbology;

FeatureCompiler::FeatureCompiler( Session* session ) :
_session( session )
{
    //nop
}

osg::Node*
FeatureCompiler::compile(FeatureCursor*        cursor,
                         const FeatureProfile* featureProfile,
                         const Style*          style)
{
    if ( !featureProfile ) {
        OE_WARN << LC << "Valid feature profile required" << std::endl;
        return 0L;
    }

    if ( !style ) {
        OE_WARN << LC << "Valid style required" << std::endl;
        return 0L;
    }

    osg::ref_ptr<osg::Node> result;

    // start by making a working copy of the feature set
    FeatureList workingSet;
    cursor->fill( workingSet );

    // create a filter context that will track feature data through the process
    FilterContext cx( _session.get() );
    cx.profile() = featureProfile;

    // only localize coordinates if the map if geocentric AND the extent is
    // less than 180 degrees.
    const MapInfo& mi = _session->getMapInfo();
    GeoExtent geoExtent = featureProfile->getExtent().transform( featureProfile->getSRS()->getGeographicSRS() );
    bool localize = mi.isGeocentric() && geoExtent.width() < 180.0;

    // transform the features into the map profile
    TransformFilter xform( mi.getProfile()->getSRS(), mi.isGeocentric() );   
    xform.setLocalizeCoordinates( localize );
    cx = xform.push( workingSet, cx );

    // go through the Style and figure out which filters to use.
    const ModelSymbol*     model     = style->getSymbol<ModelSymbol>();
    const PointSymbol*     point     = style->getSymbol<PointSymbol>();
    const LineSymbol*      line      = style->getSymbol<LineSymbol>();
    const PolygonSymbol*   polygon   = style->getSymbol<PolygonSymbol>();
    const ExtrusionSymbol* extrusion = style->getSymbol<ExtrusionSymbol>();

    // model substitution
    if ( model )
    {
        SubstituteModelFilter filter( style );
        cx = filter.push( workingSet, cx );
        result = filter.getNode();
    }

    // extruded geometry
    else if ( extrusion && ( line || polygon ) )
    {
        //todo
    }

    // simple geometry
    else if ( point || line || polygon )
    {
        BuildGeometryFilter filter( style );
        cx = filter.push( workingSet, cx );
        result = filter.getNode();
    }

    else // insufficient symbology
    {
        OE_WARN << LC << "Insufficient symbology; no geometry created" << std::endl;
    }
    
    // install the localization transform if necessary.
    if ( cx.hasReferenceFrame() )
    {
        osg::MatrixTransform* delocalizer = new osg::MatrixTransform( cx.inverseReferenceFrame() );
        delocalizer->addChild( result.get() );
        result = delocalizer;
    }

    return result.release();
}