
#include <mujoco_agent_wrapper.h>

namespace tysoc {
namespace mujoco {

    std::string enumJointToMjcType( const eJointType& type )
    {
        if ( type == eJointType::FIXED ) return ""; // fixed joints are just skipped in mjcf format
        if ( type == eJointType::REVOLUTE ) return "hinge";
        if ( type == eJointType::PRISMATIC ) return "slide";
        if ( type == eJointType::SPHERICAL ) return "ball";
        if ( type == eJointType::PLANAR ) return ""; // planar joints are not supported, so skip for now
        if ( type == eJointType::FREE ) return "free";

        std::cout << "WARNING> invalid eJointType enum given" << std::endl;

        return "";
    }

    std::string enumShapeToMjcType( const eShapeType& type )
    {
        if ( type == eShapeType::BOX ) return "box";
        if ( type == eShapeType::PLANE ) return "plane";
        if ( type == eShapeType::SPHERE ) return "sphere";
        if ( type == eShapeType::CYLINDER ) return "cylinder";
        if ( type == eShapeType::CAPSULE ) return "capsule";
        if ( type == eShapeType::ELLIPSOID ) return "ellipsoid";
        if ( type == eShapeType::MESH ) return "mesh";
        if ( type == eShapeType::HFIELD ) return "hfield"; // @todo: move to mujoco_common

        std::cout << "WARNING> invalid eShapeType enum given" << std::endl;

        return "";
    }

    std::string enumActuatorToMjcType( const eActuatorType& type )
    {
        if ( type == eActuatorType::TORQUE ) return "motor";

        std::cout << "WARNING> unsupported type of actuator: " << tysoc::toString( type ) << std::endl;

        return "";
    }

    TMjcJointWrapper::TMjcJointWrapper( mjModel* mjcModelPtr,
                                        mjData* mjcDataPtr, 
                                        TKinTreeJoint* jointPtr )
    {
        m_kinTreeJointPtr = jointPtr;
        m_mjcModelPtr = mjcModelPtr;
        m_mjcDataPtr = mjcDataPtr;

        // grab some backend-info for this joint
        m_id = mj_name2id( m_mjcModelPtr, mjOBJ_JOINT, jointPtr->name.c_str() );
        if ( m_id == -1 && jointPtr->data.type != eJointType::FIXED )
        {
            std::cout << "ERROR> joint (" << jointPtr->name << ") not linked to joint" << std::endl;
            return;
        }

        m_nqpos     = jointPtr->data.nqpos;
        m_nqvel     = jointPtr->data.nqvel;
        m_qposAdr   = m_mjcModelPtr->jnt_qposadr[m_id];
        m_qvelAdr   = m_mjcModelPtr->jnt_dofadr[m_id];
    }

    void TMjcJointWrapper::setQpos( const std::vector< TScalar >& qpos )
    {
        for ( int i = 0; i < qpos.size(); i++ )
            m_mjcDataPtr->qpos[m_qposAdr + i] = qpos[i];
    }

    void TMjcJointWrapper::setQvel( const std::vector< TScalar >& qvel )
    {
        for ( int i = 0; i < qvel.size(); i++ )
            m_mjcDataPtr->qvel[m_qvelAdr + i] = qvel[i];
    }

    bool TMjcJointWrapper::isRootJoint()
    {
        if ( !m_kinTreeJointPtr->parentBodyPtr )
        {
            std::cout << "ERROR> this joint should have a parent body" << std::endl;
            return false;
        }

        if ( m_kinTreeJointPtr->parentBodyPtr->parentBodyPtr )
            return false;

        return true;
    }

    TMjcKinTreeAgentWrapper::TMjcKinTreeAgentWrapper( TAgent* agentPtr )
        : TAgentWrapper( agentPtr )
    {
        m_mjcModelPtr   = nullptr;
        m_mjcDataPtr    = nullptr;
        m_mjcScenePtr   = nullptr;

        m_hasMadeSummary = false;

        // create the mjcf resources element for this agent
        m_mjcfResourcesPtr = new mjcf::GenericElement( "mujoco" );
        m_mjcfResourcesPtr->setAttributeString( "model", name() );
        // and declare the reference to the target mjcf (where to place the resources)
        m_mjcfTargetResourcesPtr = nullptr;

        // collect the required data
        _createMjcResourcesFromKinTree();
    }

    TMjcKinTreeAgentWrapper::~TMjcKinTreeAgentWrapper()
    {
        if ( m_mjcfResourcesPtr )
        {
            delete m_mjcfResourcesPtr;
            m_mjcfResourcesPtr = nullptr;
        }

        m_mjcModelPtr   = nullptr;
        m_mjcDataPtr    = nullptr;
        m_mjcScenePtr   = nullptr;
        m_mjcfTargetResourcesPtr = nullptr;
    }

    void TMjcKinTreeAgentWrapper::setMjcModel( mjModel* mjcModelPtr )
    {
        m_mjcModelPtr = mjcModelPtr;
    }

    void TMjcKinTreeAgentWrapper::setMjcData( mjData* mjcDataPtr )
    {
        m_mjcDataPtr = mjcDataPtr;
    }

    void TMjcKinTreeAgentWrapper::setMjcScene( mjvScene* mjcScenePtr )
    {
        m_mjcScenePtr = mjcScenePtr;
    }

    void TMjcKinTreeAgentWrapper::setMjcfTargetElm( mjcf::GenericElement* targetResourcesPtr )
    {
        m_mjcfTargetResourcesPtr = targetResourcesPtr;
    }

    void TMjcKinTreeAgentWrapper::finishedCreatingResources()
    {
        if ( !m_agentPtr )
            return;

        // collect some low-level properties of the bodies (ids and more stuff)
        for ( size_t i = 0; i < m_agentPtr->bodies.size(); i++ )
            _cacheBodyProperties( m_agentPtr->bodies[i] );

        // collect some low-level properties of the joints (ids and more stuff)
        for ( size_t i = 0; i < m_agentPtr->joints.size(); i++ )
            _cacheJointProperties( m_agentPtr->joints[i] );
    }

    void TMjcKinTreeAgentWrapper::_initializeInternal()
    {
        // Check if the caller (TMjcSimulation) set the target reference
        if ( !m_mjcfTargetResourcesPtr )
        {
            std::cout << "ERROR> mjc-sim object must pass a reference of the"
                      << " target resources to this agent" << std::endl;
            return;
        }

        // grab the mjcf resources to inject, namely the worlbody
        auto _worldBodyElmPtr = mjcf::findFirstChildByType( m_mjcfResourcesPtr, "worldbody" );
        // grab the actuators as well
        auto _actuatorsElmPtr = mjcf::findFirstChildByType( m_mjcfResourcesPtr, "actuator" );
        // and the assets
        auto _assetsElmPtr = mjcf::findFirstChildByType( m_mjcfResourcesPtr, "asset" );
        // and the sensors
        auto _sensorsElmPtr = mjcf::findFirstChildByType( m_mjcfResourcesPtr, "sensor" );
        // and the contacts
        auto _contactsElmPtr = mjcf::findFirstChildByType( m_mjcfResourcesPtr, "contact" );
        // then just add them to the children of the root
        if ( _assetsElmPtr )
        {
            // grab the assets in the target element
            auto _targetAssetsElmPtr    = mjcf::findFirstChildByType( m_mjcfTargetResourcesPtr, "asset" );
            auto _assetsInTarget        = _targetAssetsElmPtr->children;
            // grab the assets in our model element
            auto _assetsInModel = _assetsElmPtr->children;
            // create a set with the current elements in the assets list
            std::set< std::string > _currentAssets;
            for ( size_t i = 0; i < _assetsInTarget.size(); i++ )
            {
                _currentAssets.emplace( _assetsInTarget[i]->getAttributeString( "name" ) );
            }
            // and place our model assets if not already there
            for ( size_t i = 0; i < _assetsInModel.size(); i++ )
            {
                auto _assetElmName = _assetsInModel[i]->getAttributeString( "name" );
                if ( _currentAssets.find( _assetElmName ) == _currentAssets.end() )
                {
                    _targetAssetsElmPtr->children.push_back( _assetsInModel[i] );
                }
            }
        }
        if ( _contactsElmPtr )
            m_mjcfTargetResourcesPtr->children.push_back( _contactsElmPtr );
        if ( _worldBodyElmPtr )
            m_mjcfTargetResourcesPtr->children.push_back( _worldBodyElmPtr );
        if ( _actuatorsElmPtr )
            m_mjcfTargetResourcesPtr->children.push_back( _actuatorsElmPtr );
        if ( _sensorsElmPtr )
            m_mjcfTargetResourcesPtr->children.push_back( _sensorsElmPtr );
    }

    void TMjcKinTreeAgentWrapper::_resetInternal()
    {
        if ( !m_agentPtr )
            return;

        // set the qpos values set by the user (left in the abstract agent)
        for ( size_t i = 0; i < m_jointWrappers.size(); i++ )
        {
            // joint being wrapped
            auto _jointPtr = m_jointWrappers[i].jointPtr();

            // buffer for the q-values
            std::vector< TScalar > _qposs;
            std::vector< TScalar > _qvels;

            if ( m_jointWrappers[i].isRootJoint() )
            {
                if ( _jointPtr->data.type == eJointType::FREE )
                {
                    auto _position = m_agentPtr->getStartPosition();
                    auto _rotation = TMat3::toQuaternion( TMat3::fromEuler( m_agentPtr->getStartRotation() ) );

                    // use the start position ( x - y - z - qw - qx - qy - qz )
                    _qposs = { _position.x, _position.y, _position.z,
                               _rotation.w, _rotation.x, _rotation.y, _rotation.z };

                    // and no initial velocities
                    _qvels = { 0., 0., 0., 0., 0., 0. };
                }
                else if ( _jointPtr->data.type == eJointType::PRISMATIC )
                {
                    auto _position = m_agentPtr->getStartPosition();

                    if ( _jointPtr->data.axis == TVec3( 1., 0., 0. ) )
                    {
                        _qposs = { _position.x };
                        _qvels = { 0. };
                    }
                    else if ( _jointPtr->data.axis == TVec3( 0., 1., 0. ) )
                    {
                        _qposs = { _position.y };
                        _qvels = { 0. };
                    }
                    else if ( _jointPtr->data.axis == TVec3( 0., 0., 1. ) )
                    {
                        _qposs = { _position.z };
                        _qvels = { 0. };
                    }
                }
                else if ( _jointPtr->data.type == eJointType::REVOLUTE )
                {
                    auto _rotation = m_agentPtr->getStartRotation();

                    if ( _jointPtr->data.axis == TVec3( 1., 0., 0. ) )
                    {
                        _qposs = { _rotation.x };
                        _qvels = { 0. };
                    }
                    else if ( _jointPtr->data.axis == TVec3( 0., 1., 0. ) )
                    {
                        _qposs = { _rotation.y };
                        _qvels = { 0. };
                    }
                    else if ( _jointPtr->data.axis == TVec3( 0., 0., 1. ) )
                    {
                        _qposs = { _rotation.z };
                        _qvels = { 0. };
                    }
                }
            }
            else
            {
                // collect qpos from kintree-joint
                for ( int j = 0; j < _jointPtr->data.nqpos; j++ )
                    _qposs.push_back( _jointPtr->qpos[j] );

                // set qvels to zeros
                for ( int j = 0; j < _jointPtr->data.nqvel; j++ )
                    _qvels.push_back( 0. );
            }

            // and set the qposs and qvels into the backend through the wrapper
            m_jointWrappers[i].setQpos( _qposs );
            m_jointWrappers[i].setQvel( _qvels );

        }

        // reset the internal high-level resources of the kintree
        m_agentPtr->reset();

        m_hasMadeSummary = false;
    }

    void TMjcKinTreeAgentWrapper::_preStepInternal()
    {
        if ( !m_agentPtr )
            return;

        auto _kinActuators = m_agentPtr->actuators;

        for ( size_t i = 0; i < _kinActuators.size(); i++ )
        {
            utils::setActuatorCtrl( m_mjcModelPtr,
                                     m_mjcDataPtr,
                                     _kinActuators[i]->name,
                                     _kinActuators[i]->ctrlValue );
        }

        if ( !m_hasMadeSummary )
        {
            m_hasMadeSummary = true;

            /* Generate summary information *******************************************/
            TGenericParams& _summary = m_agentPtr->summary();

            // collect inertia properties
            TScalar _totalMass = 0.0f;
            auto _bodies = m_agentPtr->bodies;
            for ( size_t q = 0; q < _bodies.size(); q++ )
            {
                auto _name = _bodies[q]->name;
                auto _mass = utils::getBodyMass( m_mjcModelPtr,
                                                _name );

                _summary.set( "mass-" + _name, _mass );
                _totalMass += _mass;

                auto _inertiaDiag = utils::getBodyInertiaDiag( m_mjcModelPtr,
                                                            _name );

                _summary.set( "inertia-" + _name, _inertiaDiag );
            }

            _summary.set( "total-mass", _totalMass );
            /**************************************************************************/
        }

    }

    void TMjcKinTreeAgentWrapper::_postStepInternal()
    {
        if ( !m_agentPtr )
            return;

        auto _kinBodies = m_agentPtr->bodies;
        for ( size_t i = 0; i < _kinBodies.size(); i++ )
        {
            // grab the position from the mujoco backend
            auto _pos = utils::getBodyPosition( m_mjcModelPtr,
                                                 m_mjcDataPtr,
                                                 _kinBodies[i]->name );
            // and the rotation as well
            float _rot[9];
            utils::getBodyOrientation( m_mjcModelPtr,
                                       m_mjcDataPtr,
                                       _kinBodies[i]->name, _rot );

            // convert the position/rotation data to our format
            TVec3 _position;
            TMat3 _rotation;

            _position.x = _pos.x;
            _position.y = _pos.y;
            _position.z = _pos.z;

            _rotation.buff[0] = _rot[0];
            _rotation.buff[1] = _rot[1];
            _rotation.buff[2] = _rot[2];
            _rotation.buff[3] = _rot[3];
            _rotation.buff[4] = _rot[4];
            _rotation.buff[5] = _rot[5];
            _rotation.buff[6] = _rot[6];
            _rotation.buff[7] = _rot[7];
            _rotation.buff[8] = _rot[8];

            // then set it to the body's worldTransform
            _kinBodies[i]->worldTransform.setPosition( _position );
            _kinBodies[i]->worldTransform.setRotation( _rotation );
        }

////         // collect sensor readings
////         auto _kinSensors = m_agentPtr->sensors;
////         for ( size_t i = 0; i < _kinSensors.size(); i++ )
////         {
////             if ( _kinSensors[i]->data.type == eSensorType::PROP_JOINT )
////             {
////                 auto _kinJointSensor = dynamic_cast< TKinTreeJointSensor* >( _kinSensors[i] );
//// 
////                 std::vector< float > _readings;
////                 // grab the reading from the jointpos sensor
////                 utils::getJointSensorReading( m_mjcModelPtr,
////                                                m_mjcDataPtr,
////                                                _kinJointSensor->name + std::string( "_jointpos" ),
////                                                _readings );
////                 // and also the reading from the jointvel sensor
////                 utils::getJointSensorReading( m_mjcModelPtr,
////                                                m_mjcDataPtr,
////                                                _kinJointSensor->name + std::string( "_jointvel" ),
////                                                _readings );
////                 // and store it into the sensor for later usage
////                 _kinJointSensor->theta       = _readings[0];
////                 _kinJointSensor->thetadot    = _readings[1];
//// 
////                 // std::cout << "theta: " << _readings[0] << std::endl;
////                 // std::cout << "thetadot: " << _readings[1] << std::endl;
////             }
////             else if ( _kinSensors[i]->data.type == eSensorType::PROP_BODY )
////             {
////                 auto _kinBodySensor = dynamic_cast< TKinTreeBodySensor* >( _kinSensors[i] );
//// 
////                 std::vector< float > _readings;
////                 // grab the reading from the franelinvec sensor
////                 utils::getJointSensorReading( m_mjcModelPtr,
////                                                m_mjcDataPtr,
////                                                _kinBodySensor->name + std::string( "_framelinvel" ),
////                                                _readings );
////                 // and also the reading from the framelinacc sensor
////                 utils::getJointSensorReading( m_mjcModelPtr,
////                                                m_mjcDataPtr,
////                                                _kinBodySensor->name + std::string( "_framelinacc" ),
////                                                _readings );
////                 // and store it into the sensor for later usage
////                 _kinBodySensor->linVelocity     = { _readings[0], _readings[1], _readings[2] };
////                 _kinBodySensor->linAcceleration = { _readings[3], _readings[4], _readings[5] };
//// 
////                 // grab the forces and torques from mjData
////                 utils::getCOMForces( m_mjcModelPtr,
////                                      m_mjcDataPtr,
////                                      _kinBodySensor->bodyName,
////                                      _kinBodySensor->comForce,
////                                      _kinBodySensor->comTorque );
////             }
////         }
    }

    void TMjcKinTreeAgentWrapper::_createMjcResourcesFromKinTree()
    {
        if ( !m_mjcfResourcesPtr )
            return;

        if ( !m_agentPtr )
            return;

        auto _worldBody = new mjcf::GenericElement( "worldbody" );
        m_mjcfResourcesPtr->children.push_back( _worldBody );
        
        // Collect bodies xml data into worldbody element
        auto _rootBodyPtr = m_agentPtr->getRootBody();
        _createMjcResourcesFromBodyNode( _worldBody, _rootBodyPtr );

        // Collect all assets data into the model element
        _createMjcAssetsFromKinTree();
        // Create the default sensors (for joints and bodies)
        _createMjcSensorsFromKinTree();
        // Collect all actuators and replace the names accordingly
        _createMjcActuatorsFromKinTree();
        // Collect all contact eclusions
        _createMjcExclusionContactsFromKinTree();

        // Collect extra specifics depending of the type of data being parsed
        if ( m_agentPtr->format() == eModelFormat::MJCF )
        {
            // Collect all contacts and replace the names accordingly
            if ( mjcf::findFirstChildByType( m_mjcfModelTemplatePtr, "contact" ) )
            {
                // create the target element where we are going to place our contacts
                auto _targetContactsElmPtr = new mjcf::GenericElement( "contact" );
                m_mjcfResourcesPtr->children.push_back( _targetContactsElmPtr );
                // and grab the contacts defined by our model template
                auto _srcContactsElmPtr = mjcf::findFirstChildByType( m_mjcfModelTemplatePtr, "contact" );

                // now place them inside the target contacts element
                auto _srcContacts = _srcContactsElmPtr->children;
                for ( size_t i = 0; i < _srcContacts.size(); i++ )
                {
                    _targetContactsElmPtr->children.push_back( _srcContacts[i] );
                }
            }
        }
        else if ( m_agentPtr->format() == eModelFormat::URDF )
        {
            // Check if the root has a joint that fixes it to the world
            auto _rootJoints = _rootBodyPtr->joints;
            auto _isRootFixed = false;
            for ( size_t i = 0; i < _rootJoints.size(); i++ )
            {
                if ( _rootJoints[i]->data.type == eJointType::FREE )
                {
                    _isRootFixed = true;
                    break;
                }
            }

            // Create a free joint for the root body
            if ( !_isRootFixed )
            {
                // create the freejoint element
                auto _freeJointElmPtr = new mjcf::GenericElement( "joint" );
                // compute the appropiate unique-name
                auto _freeJointName = urdf::computeUrdfName( "joint", "free", name() );
                _freeJointElmPtr->setAttributeString( "name", _freeJointName );
                // set the type to free to give 6dof to the agent at its root body
                _freeJointElmPtr->setAttributeString( "type", "free" );
                // get the appropiate body to add this joint to
                auto _rootBodyElmPtr = mjcf::findFirstChildByType( m_mjcfResourcesPtr, "body" );
                // and add it to the rootbody
                _rootBodyElmPtr->children.push_back( _freeJointElmPtr );
                
            }
        }
        else if ( m_agentPtr->format() == eModelFormat::RLSIM )
        {
            // @WIP: Add rlsim specific functionality here
        }
    }

    void TMjcKinTreeAgentWrapper::_createMjcResourcesFromBodyNode( mjcf::GenericElement* parentElmPtr,
                                                                   TKinTreeBody* kinTreeBodyPtr )
    {
        auto _bodyElmPtr = new mjcf::GenericElement( "body" );
        _bodyElmPtr->setAttributeString( "name", kinTreeBodyPtr->name );
        if ( !kinTreeBodyPtr->parentBodyPtr )
        {
            // root should use its worldTransform directly (position and rotation of root is defined by user, not model)
            _bodyElmPtr->setAttributeVec3( "pos", kinTreeBodyPtr->worldTransform.getPosition() );
            auto _quat = TMat3::toQuaternion( kinTreeBodyPtr->worldTransform.getRotation() );
            _bodyElmPtr->setAttributeVec4( "quat", { _quat.w, _quat.x, _quat.y, _quat.z } );
        }
        else
        {
            // non-root bodies use its relative transform to the parent body
            _bodyElmPtr->setAttributeVec3( "pos", kinTreeBodyPtr->localTransformZero.getPosition() );
            auto _quat = TMat3::toQuaternion( kinTreeBodyPtr->localTransformZero.getRotation() );
            _bodyElmPtr->setAttributeVec4( "quat", { _quat.w, _quat.x, _quat.y, _quat.z } );
        }
        

        // add joints
        for ( auto _joint : kinTreeBodyPtr->joints )
        {
            if ( _joint->data.type == eJointType::FIXED )
            {
                // for mujoco it it's like a non-existent joint in xml
                continue;
            }
            else if ( _joint->data.type == eJointType::PLANAR )
            {
                std::cout << "WARNING> joint with type: " << tysoc::toString( _joint->data.type ) << " "
                          << "not supported for mujoco backend" << std::endl;
                continue;
            }

            auto _jointElmPtr = new mjcf::GenericElement( "joint" );
            _jointElmPtr->setAttributeString( "name", _joint->name );
            _jointElmPtr->setAttributeString( "type", mujoco::enumJointToMjcType( _joint->data.type ) );

            if ( _joint->data.type != eJointType::FREE ) // free joints should not have any more elements
            {
                _jointElmPtr->setAttributeVec3( "pos", _joint->data.localTransform.getPosition() );
                _jointElmPtr->setAttributeVec3( "axis", _joint->data.localTransform.getRotation() * _joint->data.axis );
                _jointElmPtr->setAttributeString( "limited", ( _joint->data.limits.x < _joint->data.limits.y ) ? "true" : "false" );
                if ( _joint->data.limits.x < _joint->data.limits.y )
                {
                    if ( _joint->data.type == eJointType::SPHERICAL )
                        _jointElmPtr->setAttributeVec2( "range", { 0, rad2degrees( _joint->data.limits.y ) } );
                    else
                        _jointElmPtr->setAttributeVec2( "range", { rad2degrees( _joint->data.limits.x ), 
                                                                   rad2degrees( _joint->data.limits.y ) } );
                }

                if ( _joint->data.stiffness != 0.0f )
                    _jointElmPtr->setAttributeFloat( "stiffness", _joint->data.stiffness );
                if ( _joint->data.armature != 0.0f )
                    _jointElmPtr->setAttributeFloat( "armature", _joint->data.armature );
                if ( _joint->data.damping != 0.0f )
                    _jointElmPtr->setAttributeFloat( "damping", _joint->data.damping );
                if ( _joint->data.ref != 0.0f )
                    _jointElmPtr->setAttributeFloat( "ref", _joint->data.ref );
            }

            _bodyElmPtr->children.push_back( _jointElmPtr );
        }

        // add geoms (from colliders, visuals are used by the visualizer)
        for ( auto _collision : kinTreeBodyPtr->collisions )
        {
            auto _geomElmPtr = new mjcf::GenericElement( "geom" );
            _geomElmPtr->setAttributeString( "name", _collision->name );
            _geomElmPtr->setAttributeString( "type", mujoco::enumShapeToMjcType( _collision->data.type ) );
            _geomElmPtr->setAttributeVec3( "pos", _collision->data.localTransform.getPosition() );
            auto _gquat = TMat3::toQuaternion( _collision->data.localTransform.getRotation() );
            _geomElmPtr->setAttributeVec4( "quat", { _gquat.w, _gquat.x, _gquat.y, _gquat.z } );
            if ( _collision->data.type != eShapeType::MESH && _collision->data.type != eShapeType::HFIELD )
                _geomElmPtr->setAttributeVec3( "size", _extractMjcSizeFromStandardSize( _collision->data ) );
            else if ( _collision->data.type == eShapeType::MESH )
                _geomElmPtr->setAttributeString( "mesh", tysoc::getFilenameNoExtensionFromFilePath( _collision->data.filename ) );

            if ( _collision->data.collisionGroup != -1 )
                _geomElmPtr->setAttributeInt( "contype", _collision->data.collisionGroup );

            if ( _collision->data.collisionMask != -1 )
                _geomElmPtr->setAttributeInt( "conaffinity", _collision->data.collisionMask );

            _geomElmPtr->setAttributeVec3( "friction", _collision->data.friction );
            _geomElmPtr->setAttributeFloat( "density", _collision->data.density );
            _geomElmPtr->setAttributeVec4( "rgba", TYSOC_DEFAULT_RGBA_COLOR );

            _bodyElmPtr->children.push_back( _geomElmPtr );
        }

        // add inertia element (only if not using default calculations, which is hint by nullptr)
        if ( kinTreeBodyPtr->inertialData.mass < TYSOC_FLOAT_EPSILON )
        {
            auto& _kinInertialData = kinTreeBodyPtr->inertialData;
            auto _inertiaElmPtr = new mjcf::GenericElement( "inertial" );
            _inertiaElmPtr->setAttributeFloat( "mass", _kinInertialData.mass );
            _inertiaElmPtr->setAttributeVec3( "pos", { 0.0f, 0.0f, 0.0f} );
            auto _iquat = TMat3::toQuaternion( _kinInertialData.localTransform.getRotation() );
            _inertiaElmPtr->setAttributeVec4( "quat", { 1.0f, 0.0f, 0.0f, 0.0f } );

            if ( _kinInertialData.ixx > TYSOC_FLOAT_EPSILON || _kinInertialData.iyy > TYSOC_FLOAT_EPSILON ||
                 _kinInertialData.izz > TYSOC_FLOAT_EPSILON || _kinInertialData.ixy > TYSOC_FLOAT_EPSILON ||
                 _kinInertialData.ixz > TYSOC_FLOAT_EPSILON || _kinInertialData.iyz > TYSOC_FLOAT_EPSILON )
            {
                if ( _kinInertialData.ixy < TYSOC_FLOAT_EPSILON &&
                     _kinInertialData.ixz < TYSOC_FLOAT_EPSILON &&
                     _kinInertialData.iyz < TYSOC_FLOAT_EPSILON )
                {
                    // diagonal inertia matrix
                    _inertiaElmPtr->setAttributeVec3( "diaginertia", 
                                                      { _kinInertialData.ixx, 
                                                        _kinInertialData.iyy, 
                                                        _kinInertialData.izz } );
                }
                else
                {
                    // full inertia matrix
                    _inertiaElmPtr->setAttributeArrayFloat( "fullinertia",
                                                            { 6, 
                                                              { _kinInertialData.ixx,
                                                                _kinInertialData.iyy,
                                                                _kinInertialData.izz,
                                                                _kinInertialData.ixy,
                                                                _kinInertialData.ixz,
                                                                _kinInertialData.iyz } } );
                }
            }

            _bodyElmPtr->children.push_back( _inertiaElmPtr );
        }

        // add this body element into the mjcf (in the correct place, given by the parent element)
        parentElmPtr->children.push_back( _bodyElmPtr );

        // and finally recursively add all bodies
        for ( auto _childBody : kinTreeBodyPtr->children )
            _createMjcResourcesFromBodyNode( _bodyElmPtr, _childBody );
    }

    void TMjcKinTreeAgentWrapper::_createMjcAssetsFromKinTree()
    {
////         auto _meshAssets = m_agentPtr->meshAssets;
////         if ( _meshAssets.size() < 1 )
////         {
////             // No mesh assets to add to the model
////             return;
////         }
//// 
////         auto _assetsElmPtr = new mjcf::GenericElement( "asset" );
////         m_mjcfResourcesPtr->children.push_back( _assetsElmPtr );
//// 
////         for ( size_t i = 0; i < _meshAssets.size(); i++ )
////         {
////             auto _meshAssetElmPtr = new mjcf::GenericElement( "mesh" );
////             _meshAssetElmPtr->setAttributeString( "name", _meshAssets[i]->name );
////             _meshAssetElmPtr->setAttributeString( "file", _meshAssets[i]->file );
//// 
////             _assetsElmPtr->children.push_back( _meshAssetElmPtr );
////         }
    }

    void TMjcKinTreeAgentWrapper::_createMjcSensorsFromKinTree()
    {
////         auto _kinSensors = m_agentPtr->sensors;
////         if ( _kinSensors.size() < 1 )
////         {
////             // no sensors to add to the model
////             return;
////         }
//// 
////         auto _sensorsElmPtr = new mjcf::GenericElement( "sensor" );
////         m_mjcfResourcesPtr->children.push_back( _sensorsElmPtr );
//// 
////         for ( size_t i = 0; i < _kinSensors.size(); i++ )
////         {
////             if ( _kinSensors[i]->data.type == "joint" )
////             {
////                 // cast to the sensor type
////                 auto _kinJointSensor = dynamic_cast< TKinTreeJointSensor* >( _kinSensors[i] );
////                 // grab the jointsensor name
////                 auto _sensorName = _kinJointSensor->name;
////                 // and the target joint name
////                 auto _targetJointName = _kinJointSensor->jointName;
//// 
////                 // create a jointpos sensor and a jointvel sensor
////                 auto _jointPosMjcSensorResource = new mjcf::GenericElement( "jointpos" );
////                 auto _jointVelMjcSensorResource = new mjcf::GenericElement( "jointvel" );
//// 
////                 // set the necessary properties (joint angular position sensor)
////                 auto _mjcJointPosSensorName = _sensorName + std::string( "_jointpos" );
////                 _jointPosMjcSensorResource->setAttributeString( "name", _mjcJointPosSensorName );
////                 _jointPosMjcSensorResource->setAttributeString( "joint", _targetJointName );
//// 
////                 // set the necessary properties (joint angular speed sensor)
////                 auto _mjcJointVelSensorName = _sensorName + std::string( "_jointvel" );
////                 _jointVelMjcSensorResource->setAttributeString( "name", _mjcJointVelSensorName );
////                 _jointVelMjcSensorResource->setAttributeString( "joint", _targetJointName );
//// 
////                 // add these to the sensor element
////                 _sensorsElmPtr->children.push_back( _jointPosMjcSensorResource );
////                 _sensorsElmPtr->children.push_back( _jointVelMjcSensorResource );
////             }
////             else if ( _kinSensors[i]->data.type == "body" )
////             {
////                 // cast to the sensor type
////                 auto _kinBodySensor = dynamic_cast< TKinTreeBodySensor* >( _kinSensors[i] );
////                 // grab the jointsensor name
////                 auto _sensorName = _kinBodySensor->name;
////                 // and target body name
////                 auto _targetBodyName = _kinBodySensor->bodyName;
//// 
////                 // create a framelinvel sensor and a framelinacc sensor
////                 auto _bodyLinVelMjcSensorResource = new mjcf::GenericElement( "framelinvel" );
////                 auto _bodyLinAccMjcSensorResource = new mjcf::GenericElement( "framelinacc" );
//// 
////                 // set the necessary properties (body's linear velocity)
////                 auto _mjcBodyVelSensorName = _sensorName + std::string( "_framelinvel" );
////                 _bodyLinVelMjcSensorResource->setAttributeString( "name", _mjcBodyVelSensorName );
////                 _bodyLinVelMjcSensorResource->setAttributeString( "objtype", "body" );// @GENERALIZE
////                 _bodyLinVelMjcSensorResource->setAttributeString( "objname", _targetBodyName );
//// 
////                 // set the necessary properties (body's linear acceleration)
////                 auto _mjcBodyAccSensorName = _sensorName + std::string( "_framelinacc" );
////                 _bodyLinAccMjcSensorResource->setAttributeString( "name", _mjcBodyAccSensorName );
////                 _bodyLinAccMjcSensorResource->setAttributeString( "objtype", "body" );// @GENERALIZE
////                 _bodyLinAccMjcSensorResource->setAttributeString( "objname", _targetBodyName );                
//// 
////                 // add these to the sensor element
////                 _sensorsElmPtr->children.push_back( _bodyLinVelMjcSensorResource );
////                 _sensorsElmPtr->children.push_back( _bodyLinAccMjcSensorResource );
////             }
////         }
    }

    void TMjcKinTreeAgentWrapper::_createMjcActuatorsFromKinTree()
    {
////         auto _kinActuators = m_agentPtr->actuators;
////         if ( _kinActuators.size() < 1 )
////         {
////             // No actuators to add to the model
////             return;
////         }
//// 
////         auto _actuatorsElmPtr = new mjcf::GenericElement( "actuator" );
////         m_mjcfResourcesPtr->children.push_back( _actuatorsElmPtr );
//// 
////         for ( size_t i = 0; i < _kinActuators.size(); i++ )
////         {
////             if ( _kinActuators[i]->jointPtr == nullptr )
////             {
////                 std::cout << "WARNING> badly defined actuator: " 
////                           << _kinActuators[i]->name 
////                           << std::endl;
////                 continue;
////             }
//// 
////             // Grab the joint actuator this actuator is using
////             auto _kinJointPtr = _kinActuators[i]->jointPtr;
//// 
////             // create the mjcf resource for this actuator
////             auto _actuatorResource = new mjcf::GenericElement( _kinActuators[i]->type );
////             // and set its properties
////             _actuatorResource->setAttributeString( "name", _kinActuators[i]->name );
////             _actuatorResource->setAttributeVec2( "ctrlrange", 
////                                                  { _kinActuators[i]->minCtrl,
////                                                    _kinActuators[i]->maxCtrl } );
////             _actuatorResource->setAttributeString( "joint", _kinJointPtr->name );
//// 
////             // @CHECK|@WIP : should change to variant with specific-params ...
////             // according to the backend (mjc allows kv, kp, but bullet does not)
//// 
////             // @GENERIC
////             if ( _kinActuators[i]->gear.ndim > 0 )
////                 _actuatorResource->setAttributeArrayFloat( "gear", _kinActuators[i]->gear );
////             // @GENERIC
////             _actuatorResource->setAttributeString( "ctrllimited", ( _kinActuators[i]->clampCtrl ) ? "true" : "false" );
////             // @GENERIC
////             if ( _kinActuators[i]->type == "position" )
////                 _actuatorResource->setAttributeFloat( "kp", _kinActuators[i]->kp );
////             else if ( _kinActuators[i]->type == "velocity" )
////                 _actuatorResource->setAttributeFloat( "kv", _kinActuators[i]->kv );
//// 
////             _actuatorsElmPtr->children.push_back( _actuatorResource );
////         }
    }

    void TMjcKinTreeAgentWrapper::_createMjcExclusionContactsFromKinTree()
    {
////         auto _kinExclusionContacts = m_agentPtr->exclusionContacts;
////         if ( _kinExclusionContacts.size() < 1 )
////         {
////             return;
////         }
//// 
////         auto _exclusionContactsElmPtr = new mjcf::GenericElement( "contact" );
////         m_mjcfResourcesPtr->children.push_back( _exclusionContactsElmPtr );
//// 
////         for ( size_t i = 0; i < _kinExclusionContacts.size(); i++ )
////         {
////             auto _exclusionPair = _kinExclusionContacts[i];
////             auto _exclusionPairElmPtr = new mjcf::GenericElement( "exclude" );
////             _exclusionPairElmPtr->setAttributeString( "body1", _exclusionPair.first );
////             _exclusionPairElmPtr->setAttributeString( "body2", _exclusionPair.second );
//// 
////             _exclusionContactsElmPtr->children.push_back( _exclusionPairElmPtr );
////         }
    }

    void TMjcKinTreeAgentWrapper::_cacheBodyProperties( TKinTreeBody* kinTreeBody )
    {
        // m_bodyWrappers.push_back( TMjcBodyWrapper() );
    }

    void TMjcKinTreeAgentWrapper::_cacheJointProperties( TKinTreeJoint* kinTreeJoint )
    {
        m_jointWrappers.push_back( TMjcJointWrapper( m_mjcModelPtr, m_mjcDataPtr, kinTreeJoint ) );
    }

    TVec3 TMjcKinTreeAgentWrapper::_extractMjcSizeFromStandardSize( const TShapeData& data )
    {
        if ( data.type == eShapeType::PLANE ) return { data.size.x, data.size.y, data.size.z };
        if ( data.type == eShapeType::SPHERE ) return { data.size.x, data.size.y, data.size.z };
        if ( data.type == eShapeType::CAPSULE ) return { data.size.x, 0.5f * data.size.y, data.size.z };
        if ( data.type == eShapeType::CYLINDER ) return { data.size.x, 0.5f * data.size.y, data.size.z };
        if ( data.type == eShapeType::BOX ) return { 0.5f * data.size.x, 0.5f * data.size.y, 0.5f * data.size.z };
        if ( data.type == eShapeType::MESH ) return { data.size.x, data.size.y, data.size.z };
        if ( data.type == eShapeType::HFIELD ) return { data.size.x, data.size.y, data.size.z };

        return data.size;
    }

    extern "C" TAgentWrapper* agent_createFromAbstract( TAgent* agentPtr )
    {
        return new TMjcKinTreeAgentWrapper( agentPtr );
    }

    extern "C" TAgentWrapper* agent_createFromFile( const std::string& name,
                                                    const std::string& filename )
    {
        return nullptr;
    }

    extern "C" TAgentWrapper* agent_createFromId( const std::string& name,
                                                  const std::string& format,
                                                  const std::string& id )
    {
        return nullptr;
    }

}}