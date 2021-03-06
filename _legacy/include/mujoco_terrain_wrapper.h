#pragma once

#include <mujoco_common.h>
#include <mujoco_utils.h>

// Pool size for the number of mjc bodies to use
#define MJC_TERRAIN_POOL_SIZE PROCEDURAL_TERRAIN_POOL_SIZE - 1
// Default parameters for the connected-paths section
#define MJC_TERRAIN_PATH_DEFAULT_WIDTH 0.5f
#define MJC_TERRAIN_PATH_DEFAULT_DEPTH 2.0f
#define MJC_TERRAIN_PATH_DEFAULT_TICKNESS 0.025f

#include <terrain_wrapper.h>

namespace tysoc {
namespace mujoco {

    /**
    * This is a wrapper on top of the primitives...
    * used by the terrain generators
    */
    struct TMjcTerrainPrimitive
    {
        int                             mjcBodyId;
        std::string                     mjcGeomName;
        std::string                     mjcGeomType;
        TVec3                           mjcGeomSize;
        std::string                     mjcGeomFilename;
        TTerrainPrimitive*     tysocPrimitiveObj;
    };


    class TMjcTerrainGenWrapper : public TTerrainGenWrapper
    {
        private :

        // Main storage for the primitives
        std::vector< TMjcTerrainPrimitive* > m_mjcTerrainPrimitives;
        // working queues for the logic
        std::queue< TMjcTerrainPrimitive* > m_mjcAvailablePrimitives;
        std::queue< TMjcTerrainPrimitive* > m_mjcWorkingPrimitives;

        // mujoco resources to inject into workspace
        mjcf::GenericElement* m_modelElmPtr;
        // mjcf data where to send the data from above
        mjcf::GenericElement* m_mjcfTargetResourcesPtr;

        // a reference to the mujoco model
        mjModel* m_mjcModelPtr;
        mjData* m_mjcDataPtr;
        mjvScene* m_mjcScenePtr;

        void _collectReusableFromGenerator();// collects primitives that can be reused and rewrapped in the lifetime of the generator
        void _collectStaticFromGenerator();// collects primitives that are single in the lifetime of the generator
        void _wrapReusablePrimitive( TTerrainPrimitive* primitivePtr );
        void _wrapStaticPrimitive( TTerrainPrimitive* primitivePtr );
        void _updateProperties( TMjcTerrainPrimitive* mjcTerrainPritimivePtr );

        TVec3 _extractMjcSizeFromStandardSize( const std::string& shapeType,
                                               const TVec3& shapeSize );

        protected :

        void _initializeInternal() override;
        void _resetInternal() override;
        void _preStepInternal() override;
        void _postStepInternal() override;

        public :

        TMjcTerrainGenWrapper( TITerrainGenerator* terrainGenPtr );
        ~TMjcTerrainGenWrapper();

        void setMjcModelRef( mjModel* mjcModelPtr );
        void setMjcDataRef( mjData* mjcDataPtr );
        void setMjcfTargetElm( mjcf::GenericElement* targetResourcesPtr );
    };


    extern "C" TTerrainGenWrapper* terrain_createFromAbstract( TITerrainGenerator* terrainGenPtr );

    extern "C" TTerrainGenWrapper* terrain_createFromParams( const std::string& name,
                                                             const TGenericParams& params );
}}