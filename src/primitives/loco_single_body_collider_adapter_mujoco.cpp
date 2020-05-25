
#include <primitives/loco_single_body_collider_adapter_mujoco.h>

namespace loco {
namespace mujoco {

    TMujocoSingleBodyColliderAdapter::TMujocoSingleBodyColliderAdapter( TSingleBodyCollider* collisionRef )
        : TISingleBodyColliderAdapter( collisionRef )
    {
        m_mjcModelRef = nullptr;
        m_mjcDataRef = nullptr;

        m_mjcGeomId = -1;
        m_mjcGeomMeshId = -1;
        m_mjcGeomHFieldId = -1;
        m_mjcGeomHFieldStartAddr = -1;
        m_mjcGeomHFieldNRows = -1;
        m_mjcGeomHFieldNCols = -1;
        m_mjcGeomRbound = 0.0;

        m_size = m_ColliderRef->size();
        m_size0 = m_ColliderRef->size();

        m_mjcfElementResources = nullptr;
        m_mjcfElementAssetResources = nullptr;

    #if defined( LOCO_CORE_USE_TRACK_ALLOCS )
        const std::string name = ( m_ColliderRef ) ? m_ColliderRef->name() : "undefined";
        if ( tinyutils::Logger::IsActive() )
            LOCO_CORE_TRACE( "Loco::Allocs: Created TMujocoSingleBodyColliderAdapter {0} @ {1}", name, tinyutils::PointerToHexAddress( this ) );
        else
            std::cout << "Loco::Allocs: Created TMujocoSingleBodyColliderAdapter " << name << " @ " << tinyutils::PointerToHexAddress( this ) << std::endl;
    #endif
    }

    TMujocoSingleBodyColliderAdapter::~TMujocoSingleBodyColliderAdapter()
    {
        m_mjcModelRef = nullptr;
        m_mjcDataRef = nullptr;

        m_mjcGeomId = -1;
        m_mjcGeomMeshId = -1;
        m_mjcGeomHFieldId = -1;
        m_mjcGeomHFieldStartAddr = -1;
        m_mjcGeomHFieldNRows = -1;
        m_mjcGeomHFieldNCols = -1;
        m_mjcGeomRbound = 0.0;

        m_mjcfElementResources = nullptr;
        m_mjcfElementAssetResources = nullptr;

    #if defined( LOCO_CORE_USE_TRACK_ALLOCS )
        const std::string name = ( m_ColliderRef ) ? m_ColliderRef->name() : "undefined";
        if ( tinyutils::Logger::IsActive() )
            LOCO_CORE_TRACE( "Loco::Allocs: Destroyed TMujocoSingleBodyColliderAdapter {0} @ {1}", name, tinyutils::PointerToHexAddress( this ) );
        else
            std::cout << "Loco::Allocs: Destroyed TMujocoSingleBodyColliderAdapter " << name << " @ " << tinyutils::PointerToHexAddress( this ) << std::endl;
    #endif
    }

    void TMujocoSingleBodyColliderAdapter::Build()
    {
        LOCO_CORE_ASSERT( m_ColliderRef, "TMujocoSingleBodyColliderAdapter::Build >>> must have a valid collison-object (got nullptr instead)" );

        m_mjcfElementResources = std::make_unique<parsing::TElement>( LOCO_MJCF_GEOM_TAG, parsing::eSchemaType::MJCF );
        m_mjcfElementResources->SetString( "name", m_ColliderRef->name() );
        m_mjcfElementResources->SetVec3( "pos", { 0.0, 0.0, 0.0 } );
        m_mjcfElementResources->SetVec4( "quat", { 1.0, 0.0, 0.0, 0.0 } );
        m_mjcfElementResources->SetString( "type", enumShape_to_mjcShape( m_ColliderRef->shape() ) );
        m_mjcfElementResources->SetInt( "contype", m_ColliderRef->collisionGroup() );
        m_mjcfElementResources->SetInt( "conaffinity", m_ColliderRef->collisionMask() );
        m_mjcfElementResources->SetVec3( "friction", m_ColliderRef->data().friction );
        auto array_size = size_to_mjcSize( m_ColliderRef->shape(), m_ColliderRef->size() );
        if ( array_size.ndim > 0 )
            m_mjcfElementResources->SetArrayFloat( "size", array_size );

        const eShapeType shape = m_ColliderRef->shape();
        if ( shape == eShapeType::MESH )
        {
            auto& mesh_data = m_ColliderRef->data().mesh_data;
            if ( mesh_data.filename != "" )
            {
                const std::string mesh_file = mesh_data.filename;
                const std::string mesh_id = tinyutils::GetFilenameNoExtension( mesh_file );
                const auto mesh_scale = m_ColliderRef->size();

                m_mjcfElementAssetResources = std::make_unique<parsing::TElement>( LOCO_MJCF_MESH_TAG, parsing::eSchemaType::MJCF );
                m_mjcfElementAssetResources->SetString( "name", mesh_id );
                m_mjcfElementAssetResources->SetString( "file", mesh_file );
                m_mjcfElementAssetResources->SetVec3( "scale", mesh_scale );
                m_mjcfElementResources->SetString( "mesh", mesh_id );
            }
            else if ( mesh_data.vertices.size() > 0 && mesh_data.faces.size() > 0 )
            {
                const std::string mesh_file = m_ColliderRef->name() + ".msh";
                const std::string mesh_id = m_ColliderRef->name() + "_asset";
                const auto mesh_scale = m_ColliderRef->size();
                const auto& mesh_vertices = mesh_data.vertices;
                const auto& mesh_faces = mesh_data.faces;
                SaveMeshToBinary( mesh_file, mesh_vertices, mesh_faces );

                m_mjcfElementAssetResources = std::make_unique<parsing::TElement>( LOCO_MJCF_MESH_TAG, parsing::eSchemaType::MJCF );
                m_mjcfElementAssetResources->SetString( "name", mesh_id );
                m_mjcfElementAssetResources->SetVec3( "scale", mesh_scale );
                m_mjcfElementAssetResources->SetString( "file", mesh_file );
                m_mjcfElementResources->SetString( "mesh", mesh_id );
            }
            else
            {
                LOCO_CORE_ERROR( "TMujocoSingleBodyColliderAdapter::Build >>> mesh-collider {0} requires either a \
                                  filename or mesh-data (vertices + faces) to be provided by the user. \
                                  Creating a dummy tetrehedron instead.", m_ColliderRef->name() );

                const std::string mesh_file = m_ColliderRef->name() + ".msh";
                const std::string mesh_id = m_ColliderRef->name() + "_asset";
                const auto mesh_scale = m_ColliderRef->size();
                mesh_data.vertices = { 0.0f, 0.0f, 0.0f,
                                       1.0f, 0.0f, 0.0f,
                                       0.0f, 1.0f, 0.0f,
                                       0.0f, 0.0f, 1.0f };
                mesh_data.faces = { 0, 2, 1,
                                    0, 1, 3,
                                    1, 2, 3,
                                    0, 3, 2 };
                const auto& mesh_vertices = mesh_data.vertices;
                const auto& mesh_faces = mesh_data.faces;
                SaveMeshToBinary( mesh_file, mesh_vertices, mesh_faces );

                m_mjcfElementAssetResources = std::make_unique<parsing::TElement>( LOCO_MJCF_MESH_TAG, parsing::eSchemaType::MJCF );
                m_mjcfElementAssetResources->SetString( "name", mesh_id );
                m_mjcfElementAssetResources->SetVec3( "scale", mesh_scale );
                m_mjcfElementAssetResources->SetString( "file", mesh_file );
                m_mjcfElementResources->SetString( "mesh", mesh_id );
            }
            // @todo: handle case when the user forgot to setup the mesh-resource properly (create mesh-cube from data)
        }
        else if ( shape == eShapeType::HFIELD )
        {
            // @todo: change hfield_data for hfield_data
            const int num_depth_samples = m_ColliderRef->data().hfield_data.nDepthSamples;
            const int num_width_samples = m_ColliderRef->data().hfield_data.nWidthSamples;
            const auto& heights = m_ColliderRef->data().hfield_data.heights;
            const std::string hfield_id = m_ColliderRef->name() + "_asset";
            const float max_height = *std::max_element( heights.cbegin(), heights.cend() );
            const TVec4 size = { 0.5f * m_ColliderRef->size().x(),
                                 0.5f * m_ColliderRef->size().y(),
                                 max_height * m_ColliderRef->size().z(),
                                 LOCO_MUJOCO_HFIELD_BASE };

            m_mjcfElementAssetResources = std::make_unique<parsing::TElement>( LOCO_MJCF_HFIELD_TAG, parsing::eSchemaType::MJCF );
            m_mjcfElementAssetResources->SetString( "name", hfield_id );
            m_mjcfElementAssetResources->SetInt( "nrow", num_depth_samples );
            m_mjcfElementAssetResources->SetInt( "ncol", num_width_samples );
            m_mjcfElementAssetResources->SetVec4( "size", size );
            m_mjcfElementResources->SetString( "hfield", hfield_id );
        }

        // If parent-body has only inertia.mass set, then we'll deal with it computing an appropriate density
        if ( auto parent_body = m_ColliderRef->parent() )
        {
            const auto& inertia = parent_body->data().inertia;
            if ( ( inertia.mass > loco::EPS ) && 
                 ( ( inertia.ixx <= loco::EPS ) || ( inertia.iyy <= loco::EPS ) || ( inertia.izz <= loco::EPS ) ||
                   ( inertia.ixy <= -loco::EPS ) || ( inertia.ixz <= -loco::EPS ) || ( inertia.iyz <= -loco::EPS ) ) )
            {
                if ( shape == eShapeType::PLANE )
                    LOCO_CORE_WARN( "TMujocoSingleBodyColliderAdapter::Build >>> can't compute inertia of plane-collider {0}", m_ColliderRef->name() );
                else if ( shape == eShapeType::MESH )
                    LOCO_CORE_WARN( "TMujocoSingleBodyColliderAdapter::Build >>> can't compute inertia of mesh-collider {0}", m_ColliderRef->name() );
                else if ( shape == eShapeType::HFIELD )
                    LOCO_CORE_WARN( "TMujocoSingleBodyColliderAdapter::Build >>> can't compute inertia of hfield-collider {0}", m_ColliderRef->name() );

                const auto volume = compute_primitive_volume( shape, m_ColliderRef->size() );
                const auto density = inertia.mass / volume;
                m_mjcfElementResources->SetFloat( "density", density );
            }
        }
    }

    void TMujocoSingleBodyColliderAdapter::Initialize()
    {
        LOCO_CORE_ASSERT( m_mjcModelRef, "TMujocoSingleBodyColliderAdapter::Initialize >>> must have a valid mjModel reference" );
        LOCO_CORE_ASSERT( m_mjcDataRef, "TMujocoSingleBodyColliderAdapter::Initialize >>> must have a valid mjData reference" );

        m_mjcGeomId = mj_name2id( m_mjcModelRef, mjOBJ_GEOM, m_ColliderRef->name().c_str() );
        if ( m_mjcGeomId < 0 )
        {
            LOCO_CORE_ERROR( "TMujocoSingleBodyColliderAdapter::Initialize >>> couldn't find associated \
                              mjc-geom for collider {0}", m_ColliderRef->name() );
            return;
        }

        m_mjcGeomRbound = m_mjcModelRef->geom_rbound[m_mjcGeomId];
        if ( m_ColliderRef->data().type == eShapeType::MESH )
        {
            m_mjcGeomMeshId = m_mjcModelRef->geom_dataid[m_mjcGeomId];
            if ( m_mjcGeomMeshId < 0 )
            {
                LOCO_CORE_ERROR( "TMujocoSingleBodyColliderAdapter::Initialize >>> couldn't link to a mjc-geom \
                                  (mesh) for mesh-collider {0}", m_ColliderRef->name() );
                return;
            }

            m_mjcGeomMeshVertNum = m_mjcModelRef->mesh_vertnum[m_mjcGeomMeshId];
            m_mjcGeomMeshFaceNum = m_mjcModelRef->mesh_facenum[m_mjcGeomMeshId];
            m_mjcGeomMeshVertStartAddr = m_mjcModelRef->mesh_vertadr[m_mjcGeomMeshId];
            m_mjcGeomMeshFaceStartAddr = m_mjcModelRef->mesh_faceadr[m_mjcGeomMeshId];
        }
        else if ( m_ColliderRef->data().type == eShapeType::HFIELD )
        {
            m_mjcGeomHFieldId = m_mjcModelRef->geom_dataid[m_mjcGeomId];
            if ( m_mjcGeomHFieldId < 0 )
            {
                LOCO_CORE_ERROR( "TMujocoSingleBodyColliderAdapter::Initialize >>> couldn' link to a mjc-geom \
                                  (hfield) for hfield-collider {0}", m_ColliderRef->name() );
                return;
            }

            m_mjcGeomHFieldNRows = m_mjcModelRef->hfield_nrow[m_mjcGeomHFieldId];
            m_mjcGeomHFieldNCols = m_mjcModelRef->hfield_ncol[m_mjcGeomHFieldId];
            m_mjcGeomHFieldStartAddr = m_mjcModelRef->hfield_adr[m_mjcGeomHFieldId];
            ChangeElevationData( m_ColliderRef->data().hfield_data.heights );
        }

        const eShapeType shape = m_ColliderRef->shape();
        LOCO_CORE_TRACE( "MuJoCo-backend >>> collision-adapter" );
        LOCO_CORE_TRACE( "\tname            : {0}", m_ColliderRef->name() );
        LOCO_CORE_TRACE( "\tcol-size        : {0}", ToString( m_ColliderRef->size() ) );
        LOCO_CORE_TRACE( "\tcol-shape       : {0}", ToString( m_ColliderRef->shape() ) );
        LOCO_CORE_TRACE( "\tcol-group       : {0}", m_ColliderRef->collisionGroup() );
        LOCO_CORE_TRACE( "\tcol-mask        : {0}", m_ColliderRef->collisionMask() );
        LOCO_CORE_TRACE( "\tmjc-geom-id     : {0}", m_mjcGeomId );
        LOCO_CORE_TRACE( "\tmjc-geom-rbound : {0}", m_mjcGeomRbound );
        if ( shape != eShapeType::MESH && shape != eShapeType::HFIELD )
            LOCO_CORE_TRACE( "\tcomputed-rbound : {0}", compute_primitive_rbound( m_ColliderRef->shape(), m_ColliderRef->size() ) );
        LOCO_CORE_TRACE( "\tmjc-contype     : {0}", m_mjcModelRef->geom_contype[m_mjcGeomId] );
        LOCO_CORE_TRACE( "\tmjc-conaffinity : {0}", m_mjcModelRef->geom_conaffinity[m_mjcGeomId] );
        LOCO_CORE_TRACE( "\tmjc-condim      : {0}", m_mjcModelRef->geom_condim[m_mjcGeomId] );
        LOCO_CORE_TRACE( "\tmjc-prnt-body-id: {0}", m_mjcModelRef->geom_bodyid[m_mjcGeomId] );
        LOCO_CORE_TRACE( "\tmjc-friction    : {0}", ToString( mjarray_to_sizef( m_mjcModelRef->geom_friction + 3 * m_mjcGeomId, 3 ) ) );

        if ( shape == eShapeType::MESH )
        {
            LOCO_CORE_TRACE( "\tmesh-filename   : {0}", m_ColliderRef->data().mesh_data.filename );
            LOCO_CORE_TRACE( "\tmjc-mesh-id     : {0}", m_mjcGeomMeshId );
            LOCO_CORE_TRACE( "\tmjc-mesh-vert-nm: {0}", m_mjcGeomMeshVertNum );
            LOCO_CORE_TRACE( "\tmjc-mesh-face-nm: {0}", m_mjcGeomMeshFaceNum );
            LOCO_CORE_TRACE( "\tmjc-mesh-vrt-adr: {0}", m_mjcGeomMeshVertStartAddr );
            LOCO_CORE_TRACE( "\tmjc-mesh-fac-adr: {0}", m_mjcGeomMeshFaceStartAddr );
        }
        else if ( shape == eShapeType::HFIELD )
        {
            LOCO_CORE_TRACE( "\tmjc-hfield-id   : {0}", m_mjcGeomHFieldId );
            LOCO_CORE_TRACE( "\tmjc-hfield-adr  : {0}", m_mjcGeomHFieldStartAddr );
            LOCO_CORE_TRACE( "\tmjc-hfield-nrows: {0}", m_mjcGeomHFieldNRows );
            LOCO_CORE_TRACE( "\tmjc-hfield-ncols: {0}", m_mjcGeomHFieldNCols );
        }

        if ( m_mjcfElementResources )
            LOCO_CORE_TRACE( "mjcf-xml          :\n{0}", m_mjcfElementResources->ToString() );
        if ( m_mjcfElementAssetResources )
            LOCO_CORE_TRACE( "mjcf-xml-asset    :\n{0}", m_mjcfElementAssetResources->ToString() );
    }

    void TMujocoSingleBodyColliderAdapter::ChangeSize( const TVec3& newSize )
    {
        m_size = newSize;

        if ( m_mjcGeomId < 0 )
        {
            LOCO_CORE_ERROR( "TMujocoSingleBodyColliderAdapter::ChangeSize >>> collider {0} not linked to a \
                              valid mjc-geom (geom-id = -1)", m_ColliderRef->name() );
            return;
        }

        const eShapeType shape = m_ColliderRef->shape();
        if ( shape == eShapeType::MESH )
            _resize_mesh( newSize );
        else if ( shape == eShapeType::HFIELD )
            _resize_hfield( newSize );
        else
            _resize_primitive( newSize );
    }

    void TMujocoSingleBodyColliderAdapter::ChangeElevationData( const std::vector<float>& heights )
    {
        if ( m_mjcGeomId < 0 )
        {
            LOCO_CORE_ERROR( "TMujocoSingleBodyColliderAdapter::ChangeElevationData >>> collider {0} not linked \
                              to a valid mjc-geom (geom-id = -1)", m_ColliderRef->name() );
            return;
        }

        if ( m_ColliderRef->shape() != eShapeType::HFIELD )
        {
            LOCO_CORE_WARN( "TMujocoSingleBodyColliderAdapter::ChangeElevationData >>> tried to set heightfield data \
                             to the non-heightfield collider {0}", m_ColliderRef->name() );
            return;
        }

        if ( ( m_mjcGeomHFieldNCols * m_mjcGeomHFieldNRows ) != heights.size() )
        {
            LOCO_CORE_WARN( "TMujocoSingleBodyColliderAdapter::ChangeElevationData >>> heights-data mismatch for \
                              collider {0}", m_ColliderRef->name() );
            LOCO_CORE_WARN( "\tndepth-samples       : {0}", m_mjcGeomHFieldNRows );
            LOCO_CORE_WARN( "\tnwidth-samples       : {0}", m_mjcGeomHFieldNCols );
            LOCO_CORE_WARN( "\texpected buffer-size : {0}", m_mjcGeomHFieldNCols * m_mjcGeomHFieldNRows );
            LOCO_CORE_WARN( "\tgiven buffer-size    : {0}", heights.size() );
            return;
        }

        const float max_height = *std::max_element( heights.cbegin(), heights.cend() );
        const size_t nx_samples = m_mjcGeomHFieldNCols;
        const size_t ny_samples = m_mjcGeomHFieldNRows;
        for ( size_t i = 0; i < ny_samples; i++ )
        {
            for ( size_t j = 0; j < nx_samples; j++ )
            {
                const size_t index = i * nx_samples + j;
                m_mjcModelRef->hfield_data[m_mjcGeomHFieldStartAddr + index] =
                    std::max( 0.0f, heights[index] / max_height );
            }
        }
        m_mjcModelRef->hfield_size[4 * m_mjcGeomHFieldId + 2] = max_height * m_size.z();
    }

    void TMujocoSingleBodyColliderAdapter::ChangeCollisionGroup( int collisionGroup )
    {
        if ( m_mjcGeomId < 0 )
        {
            LOCO_CORE_ERROR( "TMujocoSingleBodyColliderAdapter::ChangeCollisionGroup >>> collider {0} not linked \
                              to a valid mjc-geom (geom-id = -1)", m_ColliderRef->name() );
            return;
        }
        m_mjcModelRef->geom_contype[m_mjcGeomId] = collisionGroup;
    }

    void TMujocoSingleBodyColliderAdapter::ChangeCollisionMask( int collisionMask )
    {
        if ( m_mjcGeomId < 0 )
        {
            LOCO_CORE_ERROR( "TMujocoSingleBodyColliderAdapter::ChangeCollisionMask >>> collider {0} not linked \
                              to a valid mjc-geom (geom-id = -1)", m_ColliderRef->name() );
            return;
        }
        m_mjcModelRef->geom_conaffinity[m_mjcGeomId] = collisionMask;
    }

    void TMujocoSingleBodyColliderAdapter::ChangeFriction( const TScalar& friction )
    {
        if ( m_mjcGeomId < 0 )
        {
            LOCO_CORE_ERROR( "TMujocoSingleBodyColliderAdapter::ChangeFriction >>> collider {0} not linked \
                             to a valid mjc-geom (geom-id = -1", m_ColliderRef->name() );
            return;
        }
        // Update only sliding friction (leave rolling and torsional as defaults)
        m_mjcModelRef->geom_friction[3 * m_mjcGeomId + 0] = friction;
    }

    void TMujocoSingleBodyColliderAdapter::_resize_mesh( const TVec3& new_size )
    {
        // Size given by user are scales (for mesh colliders)
        const TVec3 effective_scale = { std::max( 1e-3f, new_size.x() / m_size0.x() ),
                                        std::max( 1e-3f, new_size.y() / m_size0.y() ),
                                        std::max( 1e-3f, new_size.z() / m_size0.z() ) };

        TVec3 aabb_min = { 1e6f, 1e6f, 1e6f };
        TVec3 aabb_max = { -1e6f, -1e6f, -1e6f };
        for ( size_t i = 0; i < m_mjcGeomMeshVertNum; i++ )
        {
            m_mjcModelRef->mesh_vert[3 * ( m_mjcGeomMeshVertStartAddr + i ) + 0] *= effective_scale.x();
            m_mjcModelRef->mesh_vert[3 * ( m_mjcGeomMeshVertStartAddr + i ) + 1] *= effective_scale.y();
            m_mjcModelRef->mesh_vert[3 * ( m_mjcGeomMeshVertStartAddr + i ) + 2] *= effective_scale.z();

            aabb_min.x() = std::min( aabb_min.x(), m_mjcModelRef->mesh_vert[3 * ( m_mjcGeomMeshVertStartAddr + i ) + 0] );
            aabb_min.y() = std::min( aabb_min.y(), m_mjcModelRef->mesh_vert[3 * ( m_mjcGeomMeshVertStartAddr + i ) + 1] );
            aabb_min.z() = std::min( aabb_min.z(), m_mjcModelRef->mesh_vert[3 * ( m_mjcGeomMeshVertStartAddr + i ) + 2] );

            aabb_max.x() = std::max( aabb_max.x(), m_mjcModelRef->mesh_vert[3 * ( m_mjcGeomMeshVertStartAddr + i ) + 0] );
            aabb_max.y() = std::max( aabb_max.y(), m_mjcModelRef->mesh_vert[3 * ( m_mjcGeomMeshVertStartAddr + i ) + 1] );
            aabb_max.z() = std::max( aabb_max.z(), m_mjcModelRef->mesh_vert[3 * ( m_mjcGeomMeshVertStartAddr + i ) + 2] );
        }
        //// m_mjcModelRef->geom_rbound[m_mjcGeomId] = 0.5f * ( aabb_max - aabb_min ).length();

        // @todo: check if updating mass is handled by mujoco, or should we update inertial properties
    }

    void TMujocoSingleBodyColliderAdapter::_resize_hfield( const TVec3& new_size )
    {
        // Size given by user are [x(width),y(depth),height-scale]
        const auto& heights = m_ColliderRef->data().hfield_data.heights;
        const float max_height = *std::max_element( heights.cbegin(), heights.cend() );
        m_mjcModelRef->hfield_size[4 * m_mjcGeomHFieldId + 0] = new_size.x();
        m_mjcModelRef->hfield_size[4 * m_mjcGeomHFieldId + 1] = new_size.y();
        m_mjcModelRef->hfield_size[4 * m_mjcGeomHFieldId + 2] = max_height * new_size.z();

        TVec3 aabb_min = { -0.5f * new_size.x(), -0.5f * new_size.y(), -LOCO_MUJOCO_HFIELD_BASE };
        TVec3 aabb_max = { 0.5f * new_size.x(), 0.5f * new_size.y(), max_height * new_size.z() };
        //// m_mjcModelRef->geom_rbound[m_mjcGeomId] = 0.5f * ( aabb_max - aabb_min ).length();
    }

    void TMujocoSingleBodyColliderAdapter::_resize_primitive( const TVec3& new_size )
    {
        const auto shape = m_ColliderRef->shape();
        const auto array_size = size_to_mjcSize( shape, new_size );
        for ( size_t i = 0; i < array_size.ndim; i++ )
            m_mjcModelRef->geom_size[3 * m_mjcGeomId + i] = array_size[i];
        m_mjcModelRef->geom_rbound[m_mjcGeomId] = compute_primitive_rbound( shape, new_size );
    }

}}