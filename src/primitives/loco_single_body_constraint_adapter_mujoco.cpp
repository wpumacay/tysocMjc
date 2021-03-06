
#include <primitives/loco_single_body_constraint_adapter_mujoco.h>

namespace loco {
namespace primitives {

    //********************************************************************************************//
    //                              Mujoco-Adapter Interface Impl.                                //
    //********************************************************************************************//

    TIMujocoSingleBodyConstraintAdapter::TIMujocoSingleBodyConstraintAdapter()
    {
        m_MjcModelRef = nullptr;
        m_MjcDataRef = nullptr;

        m_MjcJointQposNum = -1;
        m_MjcJointQvelNum = -1;
    }

    TIMujocoSingleBodyConstraintAdapter::~TIMujocoSingleBodyConstraintAdapter()
    {
        m_MjcModelRef = nullptr;
        m_MjcDataRef = nullptr;

        m_MjcJointQposNum = -1;
        m_MjcJointQvelNum = -1;

        m_MjcfElementsResources.clear();
    }

    std::vector<parsing::TElement*> TIMujocoSingleBodyConstraintAdapter::elements_resources()
    {
        std::vector<parsing::TElement*> mjcf_elements;
        for ( auto& mjcf_element : m_MjcfElementsResources )
            mjcf_elements.push_back( mjcf_element.get() );
        return mjcf_elements;
    }

    std::vector<const parsing::TElement*> TIMujocoSingleBodyConstraintAdapter::elements_resources() const
    {
        std::vector<const parsing::TElement*> mjcf_elements;
        for ( auto& mjcf_element : m_MjcfElementsResources )
            mjcf_elements.push_back( mjcf_element.get() );
        return mjcf_elements;
    }

    //********************************************************************************************//
    //                              Revolute-constraint Adapter Impl                              //
    //********************************************************************************************//

    void TMujocoSingleBodyRevoluteConstraintAdapter::Build()
    {
        auto revolute_constraint = dynamic_cast<TSingleBodyRevoluteConstraint*>( m_ConstraintRef );
        LOCO_CORE_ASSERT( revolute_constraint, "TMujocoSingleBodyRevoluteConstraintAdapter::Build >>> \
                          constraint reference must be of type \"Revolute\", for constraint named {0}", m_ConstraintRef->name() );

        auto mjcf_element = std::make_unique<parsing::TElement>( mujoco::LOCO_MJCF_JOINT_TAG, parsing::eSchemaType::MJCF );
        mjcf_element->SetString( "name", revolute_constraint->name() );
        mjcf_element->SetString( "type", "hinge" );
        mjcf_element->SetString( "limited", "false" );
        mjcf_element->SetVec3( "pos", TVec3( revolute_constraint->local_tf().col( 3 ) ) );
        mjcf_element->SetVec3( "axis", TMat3( revolute_constraint->local_tf() ) * revolute_constraint->axis() );
        m_MjcfElementsResources.push_back( std::move( mjcf_element ) );
    }

    void TMujocoSingleBodyRevoluteConstraintAdapter::Initialize()
    {
        LOCO_CORE_ASSERT( m_MjcModelRef, "TMujocoSingleBodyRevoluteConstraintAdapter::Initialize >>> \
                          constraint {0} must have a valid mjModel reference", m_ConstraintRef->name() );
        LOCO_CORE_ASSERT( m_MjcDataRef, "TMujocoSingleBodyRevoluteConstraintAdapter::Initialize >>> \
                          constraint {0} must have a valid mjData reference", m_ConstraintRef->name() );

        m_MjcJointId = mj_name2id( m_MjcModelRef, mjOBJ_JOINT, m_ConstraintRef->name().c_str() );
        LOCO_CORE_ASSERT( m_MjcJointId >= 0, "TMujocoSingleBodyRevoluteConstraintAdapter::Initialize >>> couldn't find \
                          associated mjc-joint for constraint {0}", m_ConstraintRef->name() );
        LOCO_CORE_ASSERT( m_MjcModelRef->jnt_type[m_MjcJointId] == mjJNT_HINGE, "TMujocoSingleBodyRevoluteConstraintAdapter::Initialize >>> \
                          joint associated to constraint {0} must be a hinge-joint", m_ConstraintRef->name() );

        m_MjcJointQposAdr = m_MjcModelRef->jnt_qposadr[m_MjcJointId];
        m_MjcJointQvelAdr = m_MjcModelRef->jnt_dofadr[m_MjcJointId];
        m_MjcJointQposNum = 1;
        m_MjcJointQvelNum = 1;
    }

    void TMujocoSingleBodyRevoluteConstraintAdapter::Reset()
    {
        LOCO_CORE_ASSERT( m_MjcJointId >= 0, "TMujocoSingleBodyRevoluteConstraintAdapter::Reset >>> \
                          constraint \"{0}\" must be linked to a valid mjc-joint", m_ConstraintRef->name() );

        m_MjcDataRef->qpos[m_MjcJointQposAdr + 0] = 0.0;
        m_MjcDataRef->qvel[m_MjcJointQvelAdr + 0] = 0.0;
    }

    void TMujocoSingleBodyRevoluteConstraintAdapter::SetHingeAngle( TScalar hinge_angle )
    {
        LOCO_CORE_ASSERT( m_MjcJointId >= 0, "TMujocoSingleBodyRevoluteConstraintAdapter::SetHingeAngle >>> \
                          constraint \"{0}\" must be linked to a valid mjc-joint", m_ConstraintRef->name() );

        m_MjcDataRef->qpos[m_MjcJointQposAdr + 0] = hinge_angle;
    }

    void TMujocoSingleBodyRevoluteConstraintAdapter::SetLimits( const TVec2& limits )
    {
        LOCO_CORE_ASSERT( m_MjcJointId >= 0, "TMujocoSingleBodyRevoluteConstraintAdapter::SetLimits >>> \
                          constraint \"{0}\" must be linked to a valid mjc-joint", m_ConstraintRef->name() );

        const bool limited = ( limits.x() > limits.y() );
        m_MjcModelRef->jnt_limited[m_MjcJointId] = limited ? 0 : 1;
        if ( limited )
        {
            m_MjcModelRef->jnt_range[2 * m_MjcJointId + 0] = limits.x();
            m_MjcModelRef->jnt_range[2 * m_MjcJointId + 1] = limits.y();
        }
    }

    void TMujocoSingleBodyRevoluteConstraintAdapter::GetHingeAngle( TScalar& dst_hinge_angle )
    {
        LOCO_CORE_ASSERT( m_MjcJointId >= 0, "TMujocoSingleBodyRevoluteConstraintAdapter::GetHingeAngle >>> \
                          constraint \"{0}\" must be linked to a valid mjc-joint", m_ConstraintRef->name() );

        dst_hinge_angle = m_MjcDataRef->qpos[m_MjcJointQposAdr + 0];
    }

    //********************************************************************************************//
    //                              Prismatic-constraint Adapter Impl                             //
    //********************************************************************************************//

    void TMujocoSingleBodyPrismaticConstraintAdapter::Build()
    {
        auto prismatic_constraint = dynamic_cast<TSingleBodyPrismaticConstraint*>( m_ConstraintRef );
        LOCO_CORE_ASSERT( prismatic_constraint, "TMujocoSingleBodyPrismaticConstraintAdapter::Build >>> \
                          constraint reference must be of type \"Prismatic\", for constraint named {0}", m_ConstraintRef->name() );

        auto mjcf_element = std::make_unique<parsing::TElement>( mujoco::LOCO_MJCF_JOINT_TAG, parsing::eSchemaType::MJCF );
        mjcf_element->SetString( "name", prismatic_constraint->name() );
        mjcf_element->SetString( "type", "slide" );
        mjcf_element->SetString( "limited", "false" );
        mjcf_element->SetVec3( "pos", TVec3( prismatic_constraint->local_tf().col( 3 ) ) );
        mjcf_element->SetVec3( "axis", TMat3( prismatic_constraint->local_tf() ) * prismatic_constraint->axis() );
        m_MjcfElementsResources.push_back( std::move( mjcf_element ) );
    }

    void TMujocoSingleBodyPrismaticConstraintAdapter::Initialize()
    {
        LOCO_CORE_ASSERT( m_MjcModelRef, "TMujocoSingleBodyPrismaticConstraintAdapter::Initialize >>> \
                          constraint {0} must have a valid mjModel reference", m_ConstraintRef->name() );
        LOCO_CORE_ASSERT( m_MjcDataRef, "TMujocoSingleBodyPrismaticConstraintAdapter::Initialize >>> \
                          constraint {0} must have a valid mjData reference", m_ConstraintRef->name() );

        m_MjcJointId = mj_name2id( m_MjcModelRef, mjOBJ_JOINT, m_ConstraintRef->name().c_str() );
        LOCO_CORE_ASSERT( m_MjcJointId >= 0, "TMujocoSingleBodyPrismaticConstraintAdapter::Initialize >>> couldn't find \
                          associated mjc-joint for constraint {0}", m_ConstraintRef->name() );
        LOCO_CORE_ASSERT( m_MjcModelRef->jnt_type[m_MjcJointId] == mjJNT_SLIDE, "TMujocoSingleBodyPrismaticConstraintAdapter::Initialize >>> \
                          joint associated to constraint {0} must be a slide-joint", m_ConstraintRef->name() );

        m_MjcJointQposAdr = m_MjcModelRef->jnt_qposadr[m_MjcJointId];
        m_MjcJointQvelAdr = m_MjcModelRef->jnt_dofadr[m_MjcJointId];
        m_MjcJointQposNum = 1;
        m_MjcJointQvelNum = 1;
    }

    void TMujocoSingleBodyPrismaticConstraintAdapter::Reset()
    {
        LOCO_CORE_ASSERT( m_MjcJointId >= 0, "TMujocoSingleBodyPrismaticConstraintAdapter::Reset >>> \
                          constraint \"{0}\" must be linked to a valid mjc-joint", m_ConstraintRef->name() );

        m_MjcDataRef->qpos[m_MjcJointQposAdr + 0] = 0.0;
        m_MjcDataRef->qvel[m_MjcJointQvelAdr + 0] = 0.0;
    }

    void TMujocoSingleBodyPrismaticConstraintAdapter::SetSlidePosition( TScalar slide_position )
    {
        LOCO_CORE_ASSERT( m_MjcJointId >= 0, "TMujocoSingleBodyPrismaticConstraintAdapter::SetHingeAngle >>> \
                          constraint \"{0}\" must be linked to a valid mjc-joint", m_ConstraintRef->name() );

        m_MjcDataRef->qpos[m_MjcJointQposAdr + 0] = slide_position;
    }

    void TMujocoSingleBodyPrismaticConstraintAdapter::SetLimits( const TVec2& limits )
    {
        LOCO_CORE_ASSERT( m_MjcJointId >= 0, "TMujocoSingleBodyPrismaticConstraintAdapter::SetLimits >>> \
                          constraint \"{0}\" must be linked to a valid mjc-joint", m_ConstraintRef->name() );

        const bool limited = ( limits.x() > limits.y() );
        m_MjcModelRef->jnt_limited[m_MjcJointId] = limited ? 0 : 1;
        if ( limited )
        {
            m_MjcModelRef->jnt_range[2 * m_MjcJointId + 0] = limits.x();
            m_MjcModelRef->jnt_range[2 * m_MjcJointId + 1] = limits.y();
        }
    }

    void TMujocoSingleBodyPrismaticConstraintAdapter::GetSlidePosition( TScalar& dst_slide_position )
    {
        LOCO_CORE_ASSERT( m_MjcJointId >= 0, "TMujocoSingleBodyPrismaticConstraintAdapter::GetHingeAngle >>> \
                          constraint \"{0}\" must be linked to a valid mjc-joint", m_ConstraintRef->name() );

        dst_slide_position = m_MjcDataRef->qpos[m_MjcJointQposAdr + 0];
    }

    //********************************************************************************************//
    //                              Spherical-constraint Adapter Impl                             //
    //********************************************************************************************//

    void TMujocoSingleBodySphericalConstraintAdapter::Build()
    {
        auto mjcf_element = std::make_unique<parsing::TElement>( mujoco::LOCO_MJCF_JOINT_TAG, parsing::eSchemaType::MJCF );
        mjcf_element->SetString( "name", m_ConstraintRef->name() );
        mjcf_element->SetString( "type", "ball" );
        mjcf_element->SetString( "limited", "false" );
        mjcf_element->SetVec3( "pos", TVec3( m_ConstraintRef->local_tf().col( 3 ) ) );
        mjcf_element->SetVec3( "axis", { 0.0f, 0.0f, 1.0f } );
        m_MjcfElementsResources.push_back( std::move( mjcf_element ) );
    }

    void TMujocoSingleBodySphericalConstraintAdapter::Initialize()
    {
        LOCO_CORE_ASSERT( m_MjcModelRef, "TMujocoSingleBodySphericalConstraintAdapter::Initialize >>> \
                          constraint {0} must have a valid mjModel reference", m_ConstraintRef->name() );
        LOCO_CORE_ASSERT( m_MjcDataRef, "TMujocoSingleBodySphericalConstraintAdapter::Initialize >>> \
                          constraint {0} must have a valid mjData reference", m_ConstraintRef->name() );

        m_MjcJointId = mj_name2id( m_MjcModelRef, mjOBJ_JOINT, m_ConstraintRef->name().c_str() );
        LOCO_CORE_ASSERT( m_MjcJointId >= 0, "TMujocoSingleBodySphericalConstraintAdapter::Initialize >>> couldn't find \
                          associated mjc-joint for constraint {0}", m_ConstraintRef->name() );
        LOCO_CORE_ASSERT( m_MjcModelRef->jnt_type[m_MjcJointId] == mjJNT_BALL, "TMujocoSingleBodySphericalConstraintAdapter::Initialize >>> \
                          joint associated to constraint {0} must be a ball-joint", m_ConstraintRef->name() );

        m_MjcJointQposAdr = m_MjcModelRef->jnt_qposadr[m_MjcJointId];
        m_MjcJointQvelAdr = m_MjcModelRef->jnt_dofadr[m_MjcJointId];
        m_MjcJointQposNum = 4;
        m_MjcJointQvelNum = 3;
    }

    void TMujocoSingleBodySphericalConstraintAdapter::Reset()
    {
        LOCO_CORE_ASSERT( m_MjcJointId >= 0, "TMujocoSingleBodySphericalConstraintAdapter::Reset >>> \
                          constraint \"{0}\" must be linked to a valid mjc-joint", m_ConstraintRef->name() );

        m_MjcDataRef->qpos[m_MjcJointQposAdr + 0] = 1.0; // w-quat
        m_MjcDataRef->qpos[m_MjcJointQposAdr + 1] = 0.0; // x-quat
        m_MjcDataRef->qpos[m_MjcJointQposAdr + 2] = 0.0; // y-quat
        m_MjcDataRef->qpos[m_MjcJointQposAdr + 3] = 0.0; // z-quat

        m_MjcDataRef->qvel[m_MjcJointQvelAdr + 0] = 0.0;
        m_MjcDataRef->qvel[m_MjcJointQvelAdr + 1] = 0.0;
        m_MjcDataRef->qvel[m_MjcJointQvelAdr + 2] = 0.0;
    }

    //********************************************************************************************//
    //                           Translational3d-constraint Adapter Impl                          //
    //********************************************************************************************//

    void TMujocoSingleBodyTranslational3dConstraintAdapter::Build()
    {
        auto mjcf_slide_axis_x = std::make_unique<parsing::TElement>( mujoco::LOCO_MJCF_JOINT_TAG, parsing::eSchemaType::MJCF );
        mjcf_slide_axis_x->SetString( "name", m_ConstraintRef->name() + "_trans_x" );
        mjcf_slide_axis_x->SetString( "type", "slide" );
        mjcf_slide_axis_x->SetString( "limited", "false" );
        mjcf_slide_axis_x->SetVec3( "pos", { 0.0f, 0.0f, 0.0f } );
        mjcf_slide_axis_x->SetVec3( "axis", { 1.0f, 0.0f, 0.0f } );
        auto mjcf_slide_axis_y = std::make_unique<parsing::TElement>( mujoco::LOCO_MJCF_JOINT_TAG, parsing::eSchemaType::MJCF );
        mjcf_slide_axis_y->SetString( "name", m_ConstraintRef->name() + "_trans_y" );
        mjcf_slide_axis_y->SetString( "type", "slide" );
        mjcf_slide_axis_y->SetString( "limited", "false" );
        mjcf_slide_axis_y->SetVec3( "pos", { 0.0f, 0.0f, 0.0f } );
        mjcf_slide_axis_y->SetVec3( "axis", { 0.0f, 1.0f, 0.0f } );
        auto mjcf_slide_axis_z = std::make_unique<parsing::TElement>( mujoco::LOCO_MJCF_JOINT_TAG, parsing::eSchemaType::MJCF );
        mjcf_slide_axis_z->SetString( "name", m_ConstraintRef->name() + "_trans_z" );
        mjcf_slide_axis_z->SetString( "type", "slide" );
        mjcf_slide_axis_z->SetString( "limited", "false" );
        mjcf_slide_axis_z->SetVec3( "pos", { 0.0f, 0.0f, 0.0f } );
        mjcf_slide_axis_z->SetVec3( "axis", { 0.0f, 0.0f, 1.0f } );
        m_MjcfElementsResources.push_back( std::move( mjcf_slide_axis_x ) );
        m_MjcfElementsResources.push_back( std::move( mjcf_slide_axis_y ) );
        m_MjcfElementsResources.push_back( std::move( mjcf_slide_axis_z ) );
    }

    void TMujocoSingleBodyTranslational3dConstraintAdapter::Initialize()
    {
        LOCO_CORE_ASSERT( m_MjcModelRef, "TMujocoSingleBodyTranslational3dConstraintAdapter::Initialize >>> \
                          constraint {0} must have a valid mjModel reference", m_ConstraintRef->name() );
        LOCO_CORE_ASSERT( m_MjcDataRef, "TMujocoSingleBodyTranslational3dConstraintAdapter::Initialize >>> \
                          constraint {0} must have a valid mjData reference", m_ConstraintRef->name() );

        m_MjcJointIdSlideX = mj_name2id( m_MjcModelRef, mjOBJ_JOINT, ( m_ConstraintRef->name() + "_trans_x" ).c_str() );
        m_MjcJointIdSlideY = mj_name2id( m_MjcModelRef, mjOBJ_JOINT, ( m_ConstraintRef->name() + "_trans_y" ).c_str() );
        m_MjcJointIdSlideZ = mj_name2id( m_MjcModelRef, mjOBJ_JOINT, ( m_ConstraintRef->name() + "_trans_z" ).c_str() );

        LOCO_CORE_ASSERT( m_MjcJointIdSlideX >= 0, "TMujocoSingleBodyTranslational3dConstraintAdapter::Initialize >>> couldn't find \
                          associated mjc-joint for constraint {0}", ( m_ConstraintRef->name() + "_trans_x" ) );
        LOCO_CORE_ASSERT( m_MjcJointIdSlideY >= 0, "TMujocoSingleBodyTranslational3dConstraintAdapter::Initialize >>> couldn't find \
                          associated mjc-joint for constraint {0}", ( m_ConstraintRef->name() + "_trans_y" ) );
        LOCO_CORE_ASSERT( m_MjcJointIdSlideZ >= 0, "TMujocoSingleBodyTranslational3dConstraintAdapter::Initialize >>> couldn't find \
                          associated mjc-joint for constraint {0}", ( m_ConstraintRef->name() + "_trans_z" ) );

        LOCO_CORE_ASSERT( m_MjcModelRef->jnt_type[m_MjcJointIdSlideX] == mjJNT_SLIDE, "TMujocoSingleBodyTranslational3dConstraintAdapter::Initialize >>> \
                          joint associated to constraint {0} must be a slide-joint", ( m_ConstraintRef->name() + "_trans_x" ) );
        LOCO_CORE_ASSERT( m_MjcModelRef->jnt_type[m_MjcJointIdSlideY] == mjJNT_SLIDE, "TMujocoSingleBodyTranslational3dConstraintAdapter::Initialize >>> \
                          joint associated to constraint {0} must be a slide-joint", ( m_ConstraintRef->name() + "_trans_y" ) );
        LOCO_CORE_ASSERT( m_MjcModelRef->jnt_type[m_MjcJointIdSlideZ] == mjJNT_SLIDE, "TMujocoSingleBodyTranslational3dConstraintAdapter::Initialize >>> \
                          joint associated to constraint {0} must be a slide-joint", ( m_ConstraintRef->name() + "_trans_z" ) );

        m_MjcJointQposAdrSlideX = m_MjcModelRef->jnt_qposadr[m_MjcJointIdSlideX];
        m_MjcJointQposAdrSlideY = m_MjcModelRef->jnt_qposadr[m_MjcJointIdSlideY];
        m_MjcJointQposAdrSlideZ = m_MjcModelRef->jnt_qposadr[m_MjcJointIdSlideZ];
        m_MjcJointQvelAdrSlideX = m_MjcModelRef->jnt_dofadr[m_MjcJointIdSlideX];
        m_MjcJointQvelAdrSlideY = m_MjcModelRef->jnt_dofadr[m_MjcJointIdSlideY];
        m_MjcJointQvelAdrSlideZ = m_MjcModelRef->jnt_dofadr[m_MjcJointIdSlideZ];
        m_MjcJointQposNum = 3;
        m_MjcJointQvelNum = 3;
    }

    void TMujocoSingleBodyTranslational3dConstraintAdapter::Reset()
    {
        LOCO_CORE_ASSERT( ( m_MjcJointIdSlideX >= 0 && m_MjcJointIdSlideY >= 0 && m_MjcJointIdSlideZ >= 0 ),
                          "TMujocoSingleBodyTranslational3dConstraintAdapter::Reset >>> \
                          constraint \"{0}\" must be linked to a valid set of 3 slide mjc-joints", m_ConstraintRef->name() );
        LOCO_CORE_ASSERT( m_ConstraintRef->parent(), "TMujocoSingleBodyTranslational3dConstraintAdapter::Reset >>> \
                          constraint \"{0}\" must have a valid single-body parent", m_ConstraintRef->name() );
        const auto position0 = m_ConstraintRef->parent()->pos0();

        m_MjcDataRef->qpos[m_MjcJointQposAdrSlideX + 0] = 0.0;
        m_MjcDataRef->qpos[m_MjcJointQposAdrSlideY + 0] = 0.0;
        m_MjcDataRef->qpos[m_MjcJointQposAdrSlideZ + 0] = 0.0;

        m_MjcDataRef->qvel[m_MjcJointQvelAdrSlideX + 0] = 0.0;
        m_MjcDataRef->qvel[m_MjcJointQvelAdrSlideY + 0] = 0.0;
        m_MjcDataRef->qvel[m_MjcJointQvelAdrSlideZ + 0] = 0.0;
    }

    //********************************************************************************************//
    //                             Universal3d-constraint Adapter Impl                            //
    //********************************************************************************************//

    void TMujocoSingleBodyUniversal3dConstraintAdapter::Build()
    {
        auto mjcf_slide_axis_x = std::make_unique<parsing::TElement>( mujoco::LOCO_MJCF_JOINT_TAG, parsing::eSchemaType::MJCF );
        mjcf_slide_axis_x->SetString( "name", m_ConstraintRef->name() + "_trans_x" );
        mjcf_slide_axis_x->SetString( "type", "slide" );
        mjcf_slide_axis_x->SetString( "limited", "false" );
        mjcf_slide_axis_x->SetVec3( "pos", { 0.0f, 0.0f, 0.0f } );
        mjcf_slide_axis_x->SetVec3( "axis", { 1.0f, 0.0f, 0.0f } );
        auto mjcf_slide_axis_y = std::make_unique<parsing::TElement>( mujoco::LOCO_MJCF_JOINT_TAG, parsing::eSchemaType::MJCF );
        mjcf_slide_axis_y->SetString( "name", m_ConstraintRef->name() + "_trans_y" );
        mjcf_slide_axis_y->SetString( "type", "slide" );
        mjcf_slide_axis_y->SetString( "limited", "false" );
        mjcf_slide_axis_y->SetVec3( "pos", { 0.0f, 0.0f, 0.0f } );
        mjcf_slide_axis_y->SetVec3( "axis", { 0.0f, 1.0f, 0.0f } );
        auto mjcf_slide_axis_z = std::make_unique<parsing::TElement>( mujoco::LOCO_MJCF_JOINT_TAG, parsing::eSchemaType::MJCF );
        mjcf_slide_axis_z->SetString( "name", m_ConstraintRef->name() + "_trans_z" );
        mjcf_slide_axis_z->SetString( "type", "slide" );
        mjcf_slide_axis_z->SetString( "limited", "false" );
        mjcf_slide_axis_z->SetVec3( "pos", { 0.0f, 0.0f, 0.0f } );
        mjcf_slide_axis_z->SetVec3( "axis", { 0.0f, 0.0f, 1.0f } );
        auto mjcf_hinge_axis_z = std::make_unique<parsing::TElement>( mujoco::LOCO_MJCF_JOINT_TAG, parsing::eSchemaType::MJCF );
        mjcf_hinge_axis_z->SetString( "name", m_ConstraintRef->name() + "_rot_z" );
        mjcf_hinge_axis_z->SetString( "type", "hinge" );
        mjcf_hinge_axis_z->SetString( "limited", "false" );
        mjcf_hinge_axis_z->SetVec3( "pos", { 0.0f, 0.0f, 0.0f } );
        mjcf_hinge_axis_z->SetVec3( "axis", { 0.0f, 0.0f, 1.0f } );
        m_MjcfElementsResources.push_back( std::move( mjcf_slide_axis_x ) );
        m_MjcfElementsResources.push_back( std::move( mjcf_slide_axis_y ) );
        m_MjcfElementsResources.push_back( std::move( mjcf_slide_axis_z ) );
        m_MjcfElementsResources.push_back( std::move( mjcf_hinge_axis_z ) );
    }

    void TMujocoSingleBodyUniversal3dConstraintAdapter::Initialize()
    {
        LOCO_CORE_ASSERT( m_MjcModelRef, "TMujocoSingleBodyUniversal3dConstraintAdapter::Initialize >>> \
                          constraint {0} must have a valid mjModel reference", m_ConstraintRef->name() );
        LOCO_CORE_ASSERT( m_MjcDataRef, "TMujocoSingleBodyUniversal3dConstraintAdapter::Initialize >>> \
                          constraint {0} must have a valid mjData reference", m_ConstraintRef->name() );

        m_MjcJointIdSlideX = mj_name2id( m_MjcModelRef, mjOBJ_JOINT, ( m_ConstraintRef->name() + "_trans_x" ).c_str() );
        m_MjcJointIdSlideY = mj_name2id( m_MjcModelRef, mjOBJ_JOINT, ( m_ConstraintRef->name() + "_trans_y" ).c_str() );
        m_MjcJointIdSlideZ = mj_name2id( m_MjcModelRef, mjOBJ_JOINT, ( m_ConstraintRef->name() + "_trans_z" ).c_str() );
        m_MjcJointIdHingeZ = mj_name2id( m_MjcModelRef, mjOBJ_JOINT, ( m_ConstraintRef->name() + "_rot_z" ).c_str() );

        LOCO_CORE_ASSERT( m_MjcJointIdSlideX >= 0, "TMujocoSingleBodyUniversal3dConstraintAdapter::Initialize >>> couldn't find \
                          associated mjc-joint for constraint {0}", ( m_ConstraintRef->name() + "_trans_x" ) );
        LOCO_CORE_ASSERT( m_MjcJointIdSlideY >= 0, "TMujocoSingleBodyUniversal3dConstraintAdapter::Initialize >>> couldn't find \
                          associated mjc-joint for constraint {0}", ( m_ConstraintRef->name() + "_trans_y" ) );
        LOCO_CORE_ASSERT( m_MjcJointIdSlideZ >= 0, "TMujocoSingleBodyUniversal3dConstraintAdapter::Initialize >>> couldn't find \
                          associated mjc-joint for constraint {0}", ( m_ConstraintRef->name() + "_trans_z" ) );
        LOCO_CORE_ASSERT( m_MjcJointIdHingeZ >= 0, "TMujocoSingleBodyUniversal3dConstraintAdapter::Initialize >>> couldn't find \
                          associated mjc-joint for constraint {0}", ( m_ConstraintRef->name() + "_rot_z" ) );

        LOCO_CORE_ASSERT( m_MjcModelRef->jnt_type[m_MjcJointIdSlideX] == mjJNT_SLIDE, "TMujocoSingleBodyUniversal3dConstraintAdapter::Initialize >>> \
                          joint associated to constraint {0} must be a slide-joint", ( m_ConstraintRef->name() + "_trans_x" ) );
        LOCO_CORE_ASSERT( m_MjcModelRef->jnt_type[m_MjcJointIdSlideY] == mjJNT_SLIDE, "TMujocoSingleBodyUniversal3dConstraintAdapter::Initialize >>> \
                          joint associated to constraint {0} must be a slide-joint", ( m_ConstraintRef->name() + "_trans_y" ) );
        LOCO_CORE_ASSERT( m_MjcModelRef->jnt_type[m_MjcJointIdSlideZ] == mjJNT_SLIDE, "TMujocoSingleBodyUniversal3dConstraintAdapter::Initialize >>> \
                          joint associated to constraint {0} must be a slide-joint", ( m_ConstraintRef->name() + "_trans_z" ) );
        LOCO_CORE_ASSERT( m_MjcModelRef->jnt_type[m_MjcJointIdHingeZ] == mjJNT_HINGE, "TMujocoSingleBodyUniversal3dConstraintAdapter::Initialize >>> \
                          joint associated to constraint {0} must be a hinge-joint", ( m_ConstraintRef->name() + "_rot_z" ) );

        m_MjcJointQposAdrSlideX = m_MjcModelRef->jnt_qposadr[m_MjcJointIdSlideX];
        m_MjcJointQposAdrSlideY = m_MjcModelRef->jnt_qposadr[m_MjcJointIdSlideY];
        m_MjcJointQposAdrSlideZ = m_MjcModelRef->jnt_qposadr[m_MjcJointIdSlideZ];
        m_MjcJointQposAdrHingeZ = m_MjcModelRef->jnt_qposadr[m_MjcJointIdHingeZ];
        m_MjcJointQvelAdrSlideX = m_MjcModelRef->jnt_dofadr[m_MjcJointIdSlideX];
        m_MjcJointQvelAdrSlideY = m_MjcModelRef->jnt_dofadr[m_MjcJointIdSlideY];
        m_MjcJointQvelAdrSlideZ = m_MjcModelRef->jnt_dofadr[m_MjcJointIdSlideZ];
        m_MjcJointQvelAdrHingeZ = m_MjcModelRef->jnt_dofadr[m_MjcJointIdHingeZ];
        m_MjcJointQposNum = 4;
        m_MjcJointQvelNum = 4;
    }

    void TMujocoSingleBodyUniversal3dConstraintAdapter::Reset()
    {
        LOCO_CORE_ASSERT( ( m_MjcJointIdSlideX >= 0 && m_MjcJointIdSlideY >= 0 && m_MjcJointIdSlideZ >= 0 && m_MjcJointIdHingeZ ),
                          "TMujocoSingleBodyUniversal3dConstraintAdapter::Reset >>> \
                          constraint \"{0}\" must be linked to a valid set of 3 slide mjc-joints", m_ConstraintRef->name() );
        LOCO_CORE_ASSERT( m_ConstraintRef->parent(), "TMujocoSingleBodyUniversal3dConstraintAdapter::Reset >>> \
                          constraint \"{0}\" must have a valid single-body parent", m_ConstraintRef->name() );
        const auto position0 = m_ConstraintRef->parent()->pos0();

        m_MjcDataRef->qpos[m_MjcJointQposAdrSlideX + 0] = 0.0;
        m_MjcDataRef->qpos[m_MjcJointQposAdrSlideY + 0] = 0.0;
        m_MjcDataRef->qpos[m_MjcJointQposAdrSlideZ + 0] = 0.0;
        m_MjcDataRef->qpos[m_MjcJointQposAdrHingeZ + 0] = 0.0; // default orientation

        m_MjcDataRef->qvel[m_MjcJointQvelAdrSlideX + 0] = 0.0;
        m_MjcDataRef->qvel[m_MjcJointQvelAdrSlideY + 0] = 0.0;
        m_MjcDataRef->qvel[m_MjcJointQvelAdrSlideZ + 0] = 0.0;
        m_MjcDataRef->qvel[m_MjcJointQvelAdrHingeZ + 0] = 0.0;
    }

    //********************************************************************************************//
    //                               Planar-constraint Adapter Impl                               //
    //********************************************************************************************//

    void TMujocoSingleBodyPlanarConstraintAdapter::Build()
    {
        auto mjcf_slide_axis_x = std::make_unique<parsing::TElement>( mujoco::LOCO_MJCF_JOINT_TAG, parsing::eSchemaType::MJCF );
        mjcf_slide_axis_x->SetString( "name", m_ConstraintRef->name() + "_trans_x" );
        mjcf_slide_axis_x->SetString( "type", "slide" );
        mjcf_slide_axis_x->SetString( "limited", "false" );
        mjcf_slide_axis_x->SetVec3( "pos", { 0.0f, 0.0f, 0.0f } );
        mjcf_slide_axis_x->SetVec3( "axis", { 1.0f, 0.0f, 0.0f } );
        auto mjcf_slide_axis_z = std::make_unique<parsing::TElement>( mujoco::LOCO_MJCF_JOINT_TAG, parsing::eSchemaType::MJCF );
        mjcf_slide_axis_z->SetString( "name", m_ConstraintRef->name() + "_trans_z" );
        mjcf_slide_axis_z->SetString( "type", "slide" );
        mjcf_slide_axis_z->SetString( "limited", "false" );
        mjcf_slide_axis_z->SetVec3( "pos", { 0.0f, 0.0f, 0.0f } );
        mjcf_slide_axis_z->SetVec3( "axis", { 0.0f, 0.0f, 1.0f } );
        auto mjcf_hinge_axis_y = std::make_unique<parsing::TElement>( mujoco::LOCO_MJCF_JOINT_TAG, parsing::eSchemaType::MJCF );
        mjcf_hinge_axis_y->SetString( "name", m_ConstraintRef->name() + "_rot_y" );
        mjcf_hinge_axis_y->SetString( "type", "hinge" );
        mjcf_hinge_axis_y->SetString( "limited", "false" );
        mjcf_hinge_axis_y->SetVec3( "pos", { 0.0f, 0.0f, 0.0f } );
        mjcf_hinge_axis_y->SetVec3( "axis", { 0.0f, 1.0f, 0.0f } );
        m_MjcfElementsResources.push_back( std::move( mjcf_slide_axis_x ) );
        m_MjcfElementsResources.push_back( std::move( mjcf_slide_axis_z ) );
        m_MjcfElementsResources.push_back( std::move( mjcf_hinge_axis_y ) );
    }

    void TMujocoSingleBodyPlanarConstraintAdapter::Initialize()
    {
        LOCO_CORE_ASSERT( m_MjcModelRef, "TMujocoSingleBodyPlanarConstraintAdapter::Initialize >>> \
                          constraint {0} must have a valid mjModel reference", m_ConstraintRef->name() );
        LOCO_CORE_ASSERT( m_MjcDataRef, "TMujocoSingleBodyPlanarConstraintAdapter::Initialize >>> \
                          constraint {0} must have a valid mjData reference", m_ConstraintRef->name() );

        m_MjcJointIdSlideX = mj_name2id( m_MjcModelRef, mjOBJ_JOINT, ( m_ConstraintRef->name() + "_trans_x" ).c_str() );
        m_MjcJointIdSlideZ = mj_name2id( m_MjcModelRef, mjOBJ_JOINT, ( m_ConstraintRef->name() + "_trans_z" ).c_str() );
        m_MjcJointIdHingeY = mj_name2id( m_MjcModelRef, mjOBJ_JOINT, ( m_ConstraintRef->name() + "_rot_y" ).c_str() );

        LOCO_CORE_ASSERT( m_MjcJointIdSlideX >= 0, "TMujocoSingleBodyPlanarConstraintAdapter::Initialize >>> couldn't find \
                          associated mjc-joint for constraint {0}", ( m_ConstraintRef->name() + "_trans_x" ) );
        LOCO_CORE_ASSERT( m_MjcJointIdSlideZ >= 0, "TMujocoSingleBodyPlanarConstraintAdapter::Initialize >>> couldn't find \
                          associated mjc-joint for constraint {0}", ( m_ConstraintRef->name() + "_trans_z" ) );
        LOCO_CORE_ASSERT( m_MjcJointIdHingeY >= 0, "TMujocoSingleBodyPlanarConstraintAdapter::Initialize >>> couldn't find \
                          associated mjc-joint for constraint {0}", ( m_ConstraintRef->name() + "_rot_y" ) );

        LOCO_CORE_ASSERT( m_MjcModelRef->jnt_type[m_MjcJointIdSlideX] == mjJNT_SLIDE, "TMujocoSingleBodyPlanarConstraintAdapter::Initialize >>> \
                          joint associated to constraint {0} must be a slide-joint", ( m_ConstraintRef->name() + "_trans_x" ) );
        LOCO_CORE_ASSERT( m_MjcModelRef->jnt_type[m_MjcJointIdSlideZ] == mjJNT_SLIDE, "TMujocoSingleBodyPlanarConstraintAdapter::Initialize >>> \
                          joint associated to constraint {0} must be a slide-joint", ( m_ConstraintRef->name() + "_trans_z" ) );
        LOCO_CORE_ASSERT( m_MjcModelRef->jnt_type[m_MjcJointIdHingeY] == mjJNT_HINGE, "TMujocoSingleBodyPlanarConstraintAdapter::Initialize >>> \
                          joint associated to constraint {0} must be a hinge-joint", ( m_ConstraintRef->name() + "_rot_y" ) );

        m_MjcJointQposAdrSlideX = m_MjcModelRef->jnt_qposadr[m_MjcJointIdSlideX];
        m_MjcJointQposAdrSlideZ = m_MjcModelRef->jnt_qposadr[m_MjcJointIdSlideZ];
        m_MjcJointQposAdrHingeY = m_MjcModelRef->jnt_qposadr[m_MjcJointIdHingeY];
        m_MjcJointQvelAdrSlideX = m_MjcModelRef->jnt_dofadr[m_MjcJointIdSlideX];
        m_MjcJointQvelAdrSlideZ = m_MjcModelRef->jnt_dofadr[m_MjcJointIdSlideZ];
        m_MjcJointQvelAdrHingeY = m_MjcModelRef->jnt_dofadr[m_MjcJointIdHingeY];
        m_MjcJointQposNum = 3;
        m_MjcJointQvelNum = 3;
    }

    void TMujocoSingleBodyPlanarConstraintAdapter::Reset()
    {
        LOCO_CORE_ASSERT( ( m_MjcJointIdSlideX >= 0 && m_MjcJointIdSlideZ >= 0 && m_MjcJointIdHingeY ),
                          "TMujocoSingleBodyPlanarConstraintAdapter::Reset >>> \
                          constraint \"{0}\" must be linked to a valid set of 3 slide mjc-joints", m_ConstraintRef->name() );
        LOCO_CORE_ASSERT( m_ConstraintRef->parent(), "TMujocoSingleBodyPlanarConstraintAdapter::Reset >>> \
                          constraint \"{0}\" must have a valid single-body parent", m_ConstraintRef->name() );
        const auto position0 = m_ConstraintRef->parent()->pos0();

        m_MjcDataRef->qpos[m_MjcJointQposAdrSlideX + 0] = 0.0;
        m_MjcDataRef->qpos[m_MjcJointQposAdrSlideZ + 0] = 0.0;
        m_MjcDataRef->qpos[m_MjcJointQposAdrHingeY + 0] = 0.0; // default orientation

        m_MjcDataRef->qvel[m_MjcJointQvelAdrSlideX + 0] = 0.0;
        m_MjcDataRef->qvel[m_MjcJointQvelAdrSlideZ + 0] = 0.0;
        m_MjcDataRef->qvel[m_MjcJointQvelAdrHingeY + 0] = 0.0;
    }
}}