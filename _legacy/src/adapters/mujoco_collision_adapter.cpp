
#include <adapters/mujoco_collision_adapter.h>

namespace tysoc {

    TMjcCollisionAdapter::TMjcCollisionAdapter( TCollision* collisionPtr )
        : TICollisionAdapter( collisionPtr )
    {
        m_mjcModelPtr           = NULL;
        m_mjcDataPtr            = NULL;
        m_mjcfXmlResource       = NULL;
        m_mjcfXmlAssetResource  = NULL;
        m_mjcGeomId             = -1;
    }

    TMjcCollisionAdapter::~TMjcCollisionAdapter()
    {
        if ( m_mjcfXmlResource )
            delete m_mjcfXmlResource;

        m_mjcGeomId  = -1;

        m_mjcModelPtr = NULL;
        m_mjcDataPtr = NULL;
        m_mjcfXmlResource = NULL;
        m_mjcfXmlAssetResource = NULL;
    }

    void TMjcCollisionAdapter::build()
    {
        assert( m_collisionPtr );

        m_mjcfXmlResource = new mjcf::GenericElement( "geom" );
        m_mjcfXmlResource->setAttributeString( "name", m_collisionPtr->name() );
        m_mjcfXmlResource->setAttributeVec3( "pos", m_collisionPtr->localPos() );
        m_mjcfXmlResource->setAttributeVec4( "quat", mujoco::quat2MjcfQuat( m_collisionPtr->localQuat() ) );
        m_mjcfXmlResource->setAttributeString( "type", mujoco::shapeType2MjcfShapeType( m_collisionPtr->shape() ) );
        m_mjcfXmlResource->setAttributeVec3( "size", mujoco::size2MjcfSize( m_collisionPtr->shape(), m_collisionPtr->size() ) );

        // @TODO: Tests these  thoroughly before exposing them for usage
        // m_mjcfXmlResource->setAttributeFloat( "density", m_collisionPtr->data().density );
        // m_mjcfXmlResource->setAttributeVec3( "friction", m_collisionPtr->data().friction );
        // m_mjcfXmlResource->setAttributeInt( "contype", m_collisionPtr->data().collisionGroup );
        // m_mjcfXmlResource->setAttributeInt( "conaffinity", m_collisionPtr->data().collisionMask );

        if ( m_collisionPtr->shape() == eShapeType::MESH )
        {
            m_mjcfXmlAssetResource = new mjcf::GenericElement( "mesh" );
            m_mjcfXmlAssetResource->setAttributeString( "name", getFilenameNoExtensionFromFilePath( m_collisionPtr->data().filename ) );
            m_mjcfXmlAssetResource->setAttributeString( "file", m_collisionPtr->data().filename );
            m_mjcfXmlAssetResource->setAttributeVec3( "scale", m_collisionPtr->size() );

            m_mjcfXmlResource->setAttributeString( "mesh", m_mjcfXmlAssetResource->getAttributeString( "name" ) );
        }

        if ( m_collisionPtr->shape() == eShapeType::HEIGHTFIELD )
        {
            auto& _hdata = m_collisionPtr->dataRef().hdata;
            m_mjcfXmlAssetResource = new mjcf::GenericElement( "hfield" );
            m_mjcfXmlAssetResource->setAttributeString( "name", m_collisionPtr->name() + "_asset" );
            m_mjcfXmlAssetResource->setAttributeInt( "nrow", _hdata.nDepthSamples );
            m_mjcfXmlAssetResource->setAttributeInt( "ncol", _hdata.nWidthSamples );

            // compute z-max for size-prop
            float _zMax = *std::max_element( _hdata.heightData.begin(), _hdata.heightData.end() );
            TVec4 _sizeprop = { 0.5f * m_collisionPtr->size().x,
                                0.5f * m_collisionPtr->size().y,
                                _zMax * m_collisionPtr->size().z, // size is zmax (because of normalization) times z-scale
                                COLLISION_DEFAULT_HFIELD_BASE };

            m_mjcfXmlAssetResource->setAttributeVec4( "size", _sizeprop );

            m_mjcfXmlResource->setAttributeString( "hfield", m_collisionPtr->name() + "_asset" );
        }
    }

    void TMjcCollisionAdapter::reset()
    {
        // nothing required for now, as the functionality exposed in the other methods seems enough
    }

    void TMjcCollisionAdapter::preStep()
    {
        // do nothing for now, because so far we only need to use the overriden methods
    }

    void TMjcCollisionAdapter::postStep()
    {
        // do nothing for now, because so far we only need to use the overriden methods
    }

    void TMjcCollisionAdapter::setLocalPosition( const TVec3& position )
    {
        assert( m_collisionPtr );
        assert( m_mjcGeomId != -1 );

        m_mjcModelPtr->geom_pos[3 * m_mjcGeomId + 0] = position.x;
        m_mjcModelPtr->geom_pos[3 * m_mjcGeomId + 1] = position.y;
        m_mjcModelPtr->geom_pos[3 * m_mjcGeomId + 2] = position.z;
    }

    void TMjcCollisionAdapter::setLocalRotation( const TMat3& rotation )
    {
        assert( m_collisionPtr );
        assert( m_mjcGeomId != -1 );

        auto _quaternion = TMat3::toQuaternion( rotation );

        m_mjcModelPtr->geom_quat[4 * m_mjcGeomId + 0] = _quaternion.w;
        m_mjcModelPtr->geom_quat[4 * m_mjcGeomId + 1] = _quaternion.x;
        m_mjcModelPtr->geom_quat[4 * m_mjcGeomId + 2] = _quaternion.y;
        m_mjcModelPtr->geom_quat[4 * m_mjcGeomId + 3] = _quaternion.z;
    }

    void TMjcCollisionAdapter::setLocalTransform( const TMat4& transform )
    {
        auto _position = transform.getPosition();
        auto _quaternion = transform.getRotQuaternion();

        // @todo: check if inertial properties are being recomputed. If not, we'll have to recompute
        //        this properties, as the inertia-matrix would have likely changed

        m_mjcModelPtr->geom_pos[3 * m_mjcGeomId + 0] = _position.x;
        m_mjcModelPtr->geom_pos[3 * m_mjcGeomId + 1] = _position.y;
        m_mjcModelPtr->geom_pos[3 * m_mjcGeomId + 2] = _position.z;

        m_mjcModelPtr->geom_quat[4 * m_mjcGeomId + 0] = _quaternion.w;
        m_mjcModelPtr->geom_quat[4 * m_mjcGeomId + 1] = _quaternion.x;
        m_mjcModelPtr->geom_quat[4 * m_mjcGeomId + 2] = _quaternion.y;
        m_mjcModelPtr->geom_quat[4 * m_mjcGeomId + 3] = _quaternion.z;
    }

    void TMjcCollisionAdapter::changeSize( const TVec3& newSize )
    {
        assert( m_collisionPtr );
        assert( m_mjcGeomId != -1 );

        if ( m_collisionPtr->shape() == eShapeType::MESH )
        {
            std::cout << "WARNING> changing mesh sizes at runtime is not supported, "
                      << "as it requires recomputing the vertex positions of the collider" << std::endl;
            return;
        }

        if ( m_collisionPtr->shape() == eShapeType::HEIGHTFIELD )
        {
            std::cout << "WARNING> changing hfield sizes at runtime is not supported, "
                      << "as it requires recomputing the elevation data of the collider" << std::endl;
            return;
        }

        auto _newSizeMjcf = mujoco::size2MjcfSize( m_collisionPtr->shape(), newSize );
        auto _newRboundMjcf = mujoco::computeRbound( m_collisionPtr->shape(), newSize );

        m_mjcModelPtr->geom_size[3 * m_mjcGeomId + 0] = _newSizeMjcf.x;
        m_mjcModelPtr->geom_size[3 * m_mjcGeomId + 1] = _newSizeMjcf.y;
        m_mjcModelPtr->geom_size[3 * m_mjcGeomId + 2] = _newSizeMjcf.z;

        m_mjcModelPtr->geom_rbound[m_mjcGeomId] = _newRboundMjcf;
    }

    void TMjcCollisionAdapter::changeElevationData( const std::vector< float >& heightData )
    {
        assert( m_collisionPtr );
        assert( m_mjcGeomId != -1 );

        if ( m_collisionPtr->shape() != eShapeType::HEIGHTFIELD )
        {
            std::cout << "WARNING> tried setting elevation data to a non-hfield collider" << std::endl;
            return;
        }

        // sanity check: make sure the given buffer has the required number of elements
        if( ( m_mjcGeomHFieldNRows * m_mjcGeomHFieldNCols ) != heightData.size() )
        {
            std::cout << "WARNING> number of elements in internal and given elevation buffers does not match" << std::endl;
            std::cout << "nx-samples    : " << m_mjcGeomHFieldNRows << std::endl;
            std::cout << "ny-samples    : " << m_mjcGeomHFieldNCols << std::endl;
            std::cout << "hdata.size()  : " << heightData.size() << std::endl;
            return;
        }

        // compute max-height to normalize the data (and force negative-heights to zero)
        const float _zMax = *std::max_element( heightData.begin(), heightData.end() );
        const int _xSamples = m_mjcGeomHFieldNCols;
        const int _ySamples = m_mjcGeomHFieldNRows;
        for ( int i = 0; i < _ySamples; i++ )
            for ( int j = 0; j < _xSamples; j++ )
                m_mjcModelPtr->hfield_data[m_mjcGeomHFieldStartAddr + (i * _xSamples + j)] = 
                                    std::max( 0.0f, heightData[i * _xSamples + j] / _zMax );

        // update size property in mjModel, as max-elevation might have changed (@todo: support to change x-y extents)
        m_mjcModelPtr->hfield_size[4 * m_mjcGeomHFieldId + 2] = _zMax * m_collisionPtr->data().size.z;
    }

    void TMjcCollisionAdapter::onResourcesCreated()
    {
        assert( m_mjcModelPtr );
        assert( m_mjcDataPtr );

        m_mjcGeomId = mj_name2id( m_mjcModelPtr, mjOBJ_GEOM, m_collisionPtr->name().c_str() );

        if ( m_mjcGeomId == -1 )
        {
            TYSOC_CORE_ERROR( "TMjcCollisionAdapter::onResourcesCreated() >>> couldn't find the associated \
                               mjc-geom for collision \"{0}\"", m_collisionPtr->name() );
            return;
        }

        m_mjcRbound = m_mjcModelPtr->geom_rbound[m_mjcGeomId];

        // @TODO: Add special functionality to handle HFIELDS and MESHES

        if ( m_collisionPtr->data().type == eShapeType::MESH )
        {
            m_mjcGeomMeshId = m_mjcModelPtr->geom_dataid[m_mjcGeomId];
        }
        else if ( m_collisionPtr->data().type == eShapeType::HEIGHTFIELD )
        {
            // hfield resource location in model structure
            m_mjcGeomHFieldId = m_mjcModelPtr->geom_dataid[m_mjcGeomId];
            // sanity check, make sure we have a valid hfield
            assert( m_mjcGeomHFieldId != -1 );

            // offset of the height data in the hdata buffer
            m_mjcGeomHFieldStartAddr = m_mjcModelPtr->hfield_adr[m_mjcGeomHFieldId];

            // grab some extra information to make sure everything is setup correctly
            m_mjcGeomHFieldNRows = m_mjcModelPtr->hfield_nrow[m_mjcGeomHFieldId];
            m_mjcGeomHFieldNCols = m_mjcModelPtr->hfield_ncol[m_mjcGeomHFieldId];
            // sanity check, make sure the rows and cols are linked correctly
            assert( m_collisionPtr->data().hdata.nWidthSamples == m_mjcGeomHFieldNRows );
            assert( m_collisionPtr->data().hdata.nDepthSamples == m_mjcGeomHFieldNCols );

            // set elevation data for the first time
            changeElevationData( m_collisionPtr->data().hdata.heightData );
        }
    }

    extern "C" TICollisionAdapter* simulation_createCollisionAdapter( TCollision* collisionPtr )
    {
        return new TMjcCollisionAdapter( collisionPtr );
    }

}