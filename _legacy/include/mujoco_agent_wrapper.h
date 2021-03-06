#pragma once

#include <mujoco_common.h>
#include <mujoco_utils.h>

#include <agent_wrapper.h>

namespace tysoc { 
namespace mujoco {

    class TMjcBodyWrapper
    {
        private :

        int m_id;
        TKinTreeBody* m_kinTreeBody;

        public :

        TMjcBodyWrapper( int id );
    };

    class TMjcJointWrapper
    {

    public :

        TMjcJointWrapper( mjModel* mjcModelPtr,
                          mjData* mjcDataPtr,
                          TKinTreeJoint* jointPtr );

        void setQpos( const std::vector< TScalar >& qpos );
        void setQvel( const std::vector< TScalar >& qvel );

        void getQpos( std::array< TScalar, TYSOC_MAX_NUM_QPOS>& qpos );
        void getQvel( std::array< TScalar, TYSOC_MAX_NUM_QVEL>& qvel );

        TKinTreeJoint* jointPtr() const { return m_kinTreeJointPtr; }
        bool isRootJoint();

    private :

        int m_id;
        int m_nqpos;
        int m_nqvel;
        int m_qposAdr;
        int m_qvelAdr;

        mjModel*    m_mjcModelPtr;
        mjData*     m_mjcDataPtr;

        TKinTreeJoint* m_kinTreeJointPtr;
    };

    class TMjcActuatorWrapper
    {

    public :

        TMjcActuatorWrapper( mjModel* mjcModelPtr,
                             mjData* mjcDataPtr,
                             TKinTreeActuator* actuatorPtr );

        void setCtrl( float value );

        TKinTreeActuator* actuatorPtr() const { return m_kinTreeActuatorPtr; }

    private :

        int m_id;

        mjModel*    m_mjcModelPtr;
        mjData*     m_mjcDataPtr;

        TKinTreeActuator* m_kinTreeActuatorPtr;
    };

    class TMjcSensorWrapper
    {

    public :

        TMjcSensorWrapper( mjModel* mjcModelPtr,
                           mjData* mjcDataPtr,
                           TKinTreeSensor* sensorPtr );
        ~TMjcSensorWrapper();

        void _initJointSensor();
        void _initBodySensor();

        float getTheta();
        float getThetaDot();

        TVec3 getLinearVelocity();
        TVec3 getLinearAcceleration();
        TVec3 getComForce();
        TVec3 getComTorque();

        TKinTreeSensor* sensorPtr() const { return m_kinTreeSensorPtr; }

    private :

        mjModel*    m_mjcModelPtr;
        mjData*     m_mjcDataPtr;

        /* resources used by joint-sensors */
        int m_mjcIdJointPos;
        int m_mjcIdJointVel;
        int m_mjcIdJointPosAdr;
        int m_mjcIdJointVelAdr;

        /* resources used by body sensors  */
        int m_mjcIdBodyLinVel;
        int m_mjcIdBodyLinAcc;
        int m_mjcIdBodyLinVelAdr;
        int m_mjcIdBodyLinAccAdr;
        int m_mjcIdBodyLinked;

        TKinTreeSensor* m_kinTreeSensorPtr;
    };

    /**
    * Agent wrapper specific for the "MuJoCo" backend. It should handle ...
    * the abstract kintree initialized by the core functionality, and use it ...
    * to create the appropiate xml data needed for mujoco to compile the required models
    * (It works in this way, so it's ok. It's fine that it does everything at the start, ...
    *  maybe it does various optimizations when compiling models :D)
    */
    class TMjcKinTreeAgentWrapper : public TAgentWrapper
    {

    public :

        TMjcKinTreeAgentWrapper( TAgent* agentPtr );
        ~TMjcKinTreeAgentWrapper();

        void build() override;

        void setMjcModelRef( mjModel* mjcModelPtr );
        void setMjcDataRef( mjData* mjcDataPtr );

        void finishedCreatingResources();

        mjcf::GenericElement* mjcfResource() const { return m_mjcfXmlResource; }
        mjcf::GenericElement* mjcfAssetResources() const { return m_mjcfXmlAssetResources; }

    protected :

        void _initializeInternal() override;
        void _resetInternal() override;
        void _preStepInternal() override;
        void _postStepInternal() override;

    private :

        mjcf::GenericElement* _createMjcResourcesFromBodyNode( TKinTreeBody* kinBody );
        mjcf::GenericElement* _createMjcResourcesFromJointNode( TKinTreeJoint* kinJoint );
        mjcf::GenericElement* _createMjcResourcesFromCollisionNode( TKinTreeCollision* kinCollision );
        mjcf::GenericElement* _createMjcResourcesFromInertialNode( const TInertialData& inertia );

        void _createMjcSensorsFromKinTree();
        void _createMjcActuatorsFromKinTree();
        void _createMjcExclusionContactsFromKinTree();
        void _createMjcAssetsFromKinTree();

        void _configureFormatMjcf();
        void _configureFormatUrdf();
        void _configureFormatRlsim();

        void _cacheBodyProperties( TKinTreeBody* kinTreeBody );
        void _cacheJointProperties( TKinTreeJoint* kinTreeJoints );
        void _cacheActuatorProperties( TKinTreeActuator* kinTreeActuator );
        void _cacheSensorProperties( TKinTreeSensor* kinTreeSensor );

        TVec3 _extractMjcSizeFromStandardSize( const TShapeData& shape );

        void _collectSummary();

    private :

        std::vector< TMjcBodyWrapper > m_bodyWrappers;
        std::vector< TMjcJointWrapper > m_jointWrappers;
        std::vector< TMjcActuatorWrapper > m_actuatorWrappers;
        std::vector< TMjcSensorWrapper > m_sensorWrappers;

        mjcf::GenericElement* m_mjcfXmlResource;
        mjcf::GenericElement* m_mjcfXmlAssetResources;

        // mujoco simulation data
        mjModel*    m_mjcModelPtr;
        mjData*     m_mjcDataPtr;
        mjvScene*   m_mjcScenePtr;

        // A flag to check if we already constructed a summary
        bool m_hasMadeSummary;

    };


    extern "C" TAgentWrapper* agent_createFromAbstract( TAgent* agentPtr );

    extern "C" TAgentWrapper* agent_createFromFile( const std::string& name,
                                                    const std::string& filename );

    extern "C" TAgentWrapper* agent_createFromId( const std::string& name,
                                                  const std::string& format,
                                                  const std::string& id );

}}