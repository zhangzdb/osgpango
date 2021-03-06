// -*-c++-*- Copyright (C) 2011 osgPango Development Team
// $Id$

#include <osg/Geode>
#include <osgUtil/TransformAttributeFunctor>
#include <osgPango/Util>
#include <osgPango/TextTransform>

namespace osgPango {

class ApplyTransformsVisitor: public osg::NodeVisitor {
public:	
	typedef std::set<osg::Drawable*> DrawableSet;

	ApplyTransformsVisitor(const osg::Matrix& matrix):
	osg::NodeVisitor (osg::NodeVisitor::TRAVERSE_ALL_CHILDREN),
	_functor         (matrix) {
	}
	
	void apply(osg::Geode& geode) {
		for(unsigned int i = 0; i < geode.getNumDrawables(); i++) {
			_drawables.insert(geode.getDrawable(i));
		}
	}

	void transform(bool coordAlign)  {
		for(
			DrawableSet::iterator i = _drawables.begin();
			i != _drawables.end();
			i++
		) {
			(*i)->accept(_functor);
			(*i)->dirtyDisplayList();
			(*i)->dirtyBound();
		
			osg::Geometry* geometry = dynamic_cast<osg::Geometry*>(*i);
		
			if(!geometry) continue;
		
			if(coordAlign) {
				// Here we do some pixel-alignment calculations. What this
				// means is that we iterate through all of vertices that make
				// up our text and make sure they occur on integer-compatible
				// coordinates. This is only important when a scale is applied
				// to the text.

				osg::Vec3Array* verts = dynamic_cast<osg::Vec3Array*>(
					geometry->getVertexArray()
				);

				if(!verts) continue;

				for(
					osg::Vec3Array::iterator v = verts->begin();
					v != verts->end();
					v++
				) {
					roundVec3(*v);
				}
			}
		
			if(geometry->getVertexArray()) geometry->getVertexArray()->dirty();
		}
	}

private:
	osgUtil::TransformAttributeFunctor _functor;
	DrawableSet                        _drawables;
};

TextTransform::TextTransform(ColorMode cm):
Text               (cm),
_positionAlignment (POS_ALIGN_LEFT_BOTTOM),
_axisAlignment     (AXIS_ALIGN_XY_PLANE),
_scale             (1.0f),
_lastTransform     (osg::Matrix::identity()),
_coordinateAlign   (COORDINATE_ALIGN_AUTO) {
}

TextTransform::TextTransform(const TextTransform& tt, const osg::CopyOp& copyOp):
Text                 (tt),
osg::MatrixTransform (tt, copyOp),
_positionAlignment   (tt._positionAlignment),
_axisAlignment       (tt._axisAlignment),
_scale               (tt._scale),
_lastTransform       (tt._lastTransform),
_coordinateAlign     (tt._coordinateAlign) {
}

bool TextTransform::finalize() {
	if(!_finalizeGeometry(this)) return false;

	calculatePosition();
	
	return true;
}

void TextTransform::clear() {
	Text::clear();

	_scale         = 1.0f;
	_lastTransform = osg::Matrix::identity();

	removeChild(0, getNumChildren());
}

void TextTransform::calculatePosition() {
	osg::Matrix alignmentTransform = getAlignmentTransform();

	ApplyTransformsVisitor nv(_lastTransform * alignmentTransform);

	this->accept(nv);

	nv.transform(isCoordinateAligned());

	_lastTransform = osg::Matrix::inverse(alignmentTransform);
}

bool TextTransform::isCoordinateAligned() const {
	bool coordAlign = _coordinateAlign == COORDINATE_ALIGN_ALWAYS;

	if(_coordinateAlign == COORDINATE_ALIGN_AUTO) {
		coordAlign = fmodf(_scale, static_cast<int>(_scale)) == 0.0f;
	}

	return coordAlign;
}

osg::Matrix TextTransform::getAlignmentTransform() const {
	// We need to have been finalized at least once to calculate a proper position.
	if(!_finalized) return osg::Matrix::identity();

	osg::Vec3   origin(getOriginTranslated(), 0.0f);
	osg::Vec3   originBaseline(getOriginBaseline(), 0.0f);
	osg::Vec3   size(getSize(), 0.0f);
	osg::Matrix axisMatrix(osg::Matrix::identity());
	osg::Matrix scaleMatrix(osg::Matrix::scale(osg::Vec3(_scale, _scale, 1.0f)));

	if(_positionAlignment == POS_ALIGN_CENTER_BOTTOM) 
		origin.x() -= size.x() / 2.0f
	;

	else if(_positionAlignment == POS_ALIGN_RIGHT_BOTTOM)
		origin.x() -= size.x()
	;

	else if(_positionAlignment == POS_ALIGN_RIGHT_CENTER) origin -= osg::Vec3(
		size.x(),
		size.y() / 2.0f,
		0.0f
	);

	else if(_positionAlignment == POS_ALIGN_RIGHT_TOP)
		origin -= size
	;

	else if(_positionAlignment == POS_ALIGN_CENTER_TOP) origin -= osg::Vec3(
		size.x() / 2.0f,
		size.y(),
		0.0f
	);

	else if(_positionAlignment == POS_ALIGN_LEFT_TOP)
		origin.y() -= size.y()
	;

	else if(_positionAlignment == POS_ALIGN_LEFT_CENTER)
		origin.y() -= size.y() / 2.0f
	;

	else if(_positionAlignment == POS_ALIGN_CENTER_CENTER) origin += osg::Vec3(
		-size.x() / 2.0f,
		-size.y() / 2.0f,
		0.0f
	);

	// TODO: We call origin.set here, but we modify the exisiting origin in
	// the previous calls; why?
	else if(_positionAlignment == POS_ALIGN_LEFT_BASE_LINE) origin.set(
		originBaseline.x(),
		originBaseline.y(),
		0.0f
	);

	else if(_positionAlignment == POS_ALIGN_CENTER_BASE_LINE) origin.set(
		originBaseline.x() - size.x() / 2.0f,
		originBaseline.y(),
		0.0f
	);

	else if(_positionAlignment == POS_ALIGN_RIGHT_BASE_LINE) origin.set(
		originBaseline.x() - size.x(),
		originBaseline.y(),
		0.0f
	);

	origin *= _scale;
	
	// If we want coordinate alignment, do it now for the origin.
	if(isCoordinateAligned()) roundVec3(origin);

	// Handle _axisAlignment...
	if(_axisAlignment == AXIS_ALIGN_XZ_PLANE) axisMatrix = osg::Matrix(
		 1.0f,  0.0f,  0.0f,  0.0f,
		 0.0f,  0.0f,  1.0f,  0.0f,
		 0.0f,  1.0f,  0.0f,  0.0f,
		 0.0f,  0.0f,  0.0f,  1.0f
	);

	else if(_axisAlignment == AXIS_ALIGN_REVERSED_XZ_PLANE) axisMatrix = osg::Matrix(
		-1.0f,  0.0f,  0.0f,  0.0f,
		 0.0f,  0.0f,  1.0f,  0.0f,
		 0.0f,  1.0f,  0.0f,  0.0f,
		 0.0f,  0.0f,  0.0f,  1.0f
	); 

	else if(_axisAlignment == AXIS_ALIGN_YZ_PLANE) axisMatrix = osg::Matrix(
		 0.0f,  1.0f,  0.0f, 0.0f,
		 0.0f,  0.0f,  1.0f, 0.0f,
		 1.0f,  0.0f,  0.0f, 0.0f,
		 0.0f,  0.0f,  0.0f, 1.0f
	);

	else if(_axisAlignment == AXIS_ALIGN_REVERSED_YZ_PLANE) axisMatrix = osg::Matrix(
		 0.0f, -1.0f,  0.0f, 0.0f,
		 0.0f,  0.0f,  1.0f, 0.0f,
		 1.0f,  0.0f,  0.0f, 0.0f,
		 0.0f,  0.0f,  0.0f, 1.0f
	); 

	else if(_axisAlignment == AXIS_ALIGN_REVERSED_XY_PLANE) axisMatrix = osg::Matrix(
		-1.0f,  0.0f,  0.0f, 0.0f,
		 0.0f,  1.0f,  0.0f, 0.0f,
		 0.0f,  0.0f,  1.0f, 0.0f,
		 0.0f,  0.0f,  0.0f, 1.0f
	);
	
	return scaleMatrix * osg::Matrix::translate(origin) * axisMatrix;
}

}

