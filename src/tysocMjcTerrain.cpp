
#include <tysocMjcTerrain.h>


namespace tysocMjc
{


    TMjcTerrainGenWrapper::TMjcTerrainGenWrapper( const std::string& name,
                                                  tysocterrain::TTerrainGenerator* terrainGenPtr )
    {
        m_name = name;
        m_terrainGenPtr = terrainGenPtr;
        m_mjcModelPtr = NULL;

        // initialize mujoco resources
        for ( size_t i = 0; i < MJC_TERRAIN_POOL_SIZE; i++ )
        {
            auto _mjcPrimitive = new TMjcTerrainPrimitive();

            _mjcPrimitive->mjcBodyId        = -1;
            _mjcPrimitive->mjcBodyName      = std::string( "terrainGen_" ) + name + std::string( "_" ) + std::to_string( i );
            _mjcPrimitive->mjcGeomType      = "box";
            _mjcPrimitive->mjcGeomSize      = { 3, { 0.5f * MJC_TERRAIN_PATH_DEFAULT_WIDTH, 
                                                     0.5f * MJC_TERRAIN_PATH_DEFAULT_DEPTH, 
                                                     0.5f * MJC_TERRAIN_PATH_DEFAULT_TICKNESS } };
            _mjcPrimitive->isAvailable      = true;

            _mjcPrimitive->tysocPrimitiveObj = NULL;

            m_mjcTerrainPrimitives.push_back( _mjcPrimitive );
            m_mjcAvailablePrimitives.push( _mjcPrimitive );
        }
    }

    TMjcTerrainGenWrapper::~TMjcTerrainGenWrapper()
    {
        if ( m_terrainGenPtr )
        {
            // deletion of the base reosurces is in charge of the scenario
            m_terrainGenPtr = NULL;
        }

        m_mjcModelPtr = NULL;

        while ( !m_mjcAvailablePrimitives.empty() )
        {
            m_mjcAvailablePrimitives.pop();
        }

        while ( !m_mjcWorkingPrimitives.empty() )
        {
            m_mjcWorkingPrimitives.pop();
        }

        for ( size_t i = 0; i < m_mjcTerrainPrimitives.size(); i++ )
        {
            delete m_mjcTerrainPrimitives[i];
            m_mjcTerrainPrimitives[i] = NULL;
        }
        m_mjcTerrainPrimitives.clear();
    }

    void TMjcTerrainGenWrapper::initialize()
    {
        // collect starting info from generator
        _collectFromGenerator();
    }

    void TMjcTerrainGenWrapper::setMjcModel( mjModel* mjcModelPtr )
    {
        m_mjcModelPtr = mjcModelPtr;
    }

    void TMjcTerrainGenWrapper::injectMjcResources( mjcf::GenericElement* root )
    {
        // create a worldbody element to inject into the root element
        auto _worldbody = new mjcf::GenericElement( "worldbody" );

        // add all geometry resources into this element
        for ( size_t i = 0; i < m_mjcTerrainPrimitives.size(); i++ )
        {
            auto _bodyElm = mjcf::_createBody( m_mjcTerrainPrimitives[i]->mjcBodyName,
                                               { 0.0f,
                                                 0.0f,
                                                 100.0f + i * ( MJC_TERRAIN_PATH_DEFAULT_TICKNESS + 1.0f ) } );

            auto _geomElm = mjcf::_createGeometry( m_mjcTerrainPrimitives[i]->mjcBodyName,
                                                   m_mjcTerrainPrimitives[i]->mjcGeomType,
                                                   m_mjcTerrainPrimitives[i]->mjcGeomSize,
                                                   1.0f );
            _geomElm->setAttributeInt( "contype", 0 );
            _geomElm->setAttributeInt( "conaffinity", 1 );

            _bodyElm->children.push_back( _geomElm );
            _worldbody->children.push_back( _bodyElm );
        }


        root->children.push_back( _worldbody );
    }

    void TMjcTerrainGenWrapper::preStep()
    {
        _collectFromGenerator();

        // update the properties of all objects (if they have a primitive as reference)
        for ( size_t i = 0; i < m_mjcTerrainPrimitives.size(); i++ )
        {
            if ( m_mjcTerrainPrimitives[i]->tysocPrimitiveObj )
            {
                _updateProperties( m_mjcTerrainPrimitives[i] );
            }
        }
    }

    void TMjcTerrainGenWrapper::_collectFromGenerator()
    {
        auto _newPrimitivesQueue = m_terrainGenPtr->getJustCreated();

        while ( !_newPrimitivesQueue.empty() )
        {
            auto _newPrimitive = _newPrimitivesQueue.front();
            _newPrimitivesQueue.pop();

            _wrapNewPrimitive( _newPrimitive );
        }

        // flush the creation queue to avoid double references everywhere
        m_terrainGenPtr->flushJustCreatedQueue();
    }

    void TMjcTerrainGenWrapper::_updateProperties( TMjcTerrainPrimitive* mjcTerrainPritimivePtr )
    {
        auto _primitiveObj = mjcTerrainPritimivePtr->tysocPrimitiveObj;


        mjcint::setBodyPosition( m_mjcModelPtr, 
                                 mjcTerrainPritimivePtr->mjcBodyName,
                                 { _primitiveObj->pos.x, _primitiveObj->pos.y, _primitiveObj->pos.z } );

        mjcint::setBodyOrientation( m_mjcModelPtr,
                                    mjcTerrainPritimivePtr->mjcBodyName,
                                    _primitiveObj->rotmat );

        mjcint::changeSize( m_mjcModelPtr,
                            mjcTerrainPritimivePtr->mjcBodyName,
                            { 0.5f * _primitiveObj->size.x, 0.5f * _primitiveObj->size.y, 0.5f * _primitiveObj->size.z } );

        mjcint::setRbound( m_mjcModelPtr,
                           mjcTerrainPritimivePtr->mjcBodyName,
                           _primitiveObj->rbound );

    }

    void TMjcTerrainGenWrapper::_wrapNewPrimitive( tysocterrain::TTerrainPrimitive* primitivePtr )
    {
        // if the pool is empty, force to recycle the last object
        if ( m_mjcAvailablePrimitives.empty() )
        {
            auto _oldest = m_mjcWorkingPrimitives.front();
            if ( _oldest->tysocPrimitiveObj )
            {
                m_terrainGenPtr->recycle( _oldest->tysocPrimitiveObj );
                _oldest->tysocPrimitiveObj = NULL;
                m_mjcWorkingPrimitives.pop();
            }
            m_mjcAvailablePrimitives.push( _oldest );
        }

        // grab the oldest available object
        auto _mjcPrimitive = m_mjcAvailablePrimitives.front();
        m_mjcAvailablePrimitives.pop();

        // link an object from the working queue with the requested new primitive
        _mjcPrimitive->tysocPrimitiveObj = primitivePtr;

        // put it into the working queue
        m_mjcWorkingPrimitives.push( _mjcPrimitive );
    }


}