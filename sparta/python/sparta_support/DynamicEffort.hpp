// <PipelineExport> -*- C++ -*-


/*!
 * \file DynamicEffort.h
 * \brief "sparta" python export class
 */

#pragma once

namespace rdp = sparta::dynamic_pipeline;
using GU = rdp::GenericUnit;
using GUPS = GU::GenericUnitParameterSet;
using GUFactory = sparta::ResourceFactory<GU, GUPS>;

class_<sparta::Resource, boost::noncopyable>
    ("Resource", init<sparta::TreeNode*, const std::string&>());

class_<sparta::Unit, bases<sparta::Resource>, boost::noncopyable>
    ("Unit", init<sparta::TreeNode*, const std::string&>());

class_<rdp::GenericUnit::GenericUnitParameterSet, bases<sparta::ParameterSet>, boost::noncopyable>
    ("GenericUnitParameterSet", init<sparta::TreeNode*>());

class_<rdp::GenericUnit, bases<sparta::Unit>, boost::noncopyable>
    ("GenericUnit", init<const std::string&, sparta::TreeNode*, rdp::GenericUnit::GenericUnitParameterSet*>())
    .add_property("name", make_function(&rdp::GenericUnit::getName,
                                        return_value_policy<copy_const_reference>()))
    .add_property("ports", make_function(+[](rdp::GenericUnit& gu){ return gu.getPortSet(); },
                                         return_value_policy<WrapperCache<sparta::PortSet>>()))
    .add_property("events", make_function(+[](rdp::GenericUnit& gu){ return gu.getEventSet(); },
                                          return_value_policy<WrapperCache<sparta::EventSet>>()))
    .add_property("stats", make_function(+[](rdp::GenericUnit& gu){ return gu.getStatisticSet(); },
                                         return_value_policy<WrapperCache<sparta::StatisticSet>>()))
    .add_property("clock", make_function(+[](rdp::GenericUnit& gu){ return gu.getClock(); },
                                         return_value_policy<WrapperCache<sparta::Clock>>()));

class_<rdp::GenericResourceFactory, boost::noncopyable>
    ("GenericResourceFactory", init<>())
    .add_property("factory", make_function(&rdp::GenericResourceFactory::getGUFactory,
                                           return_value_policy<WrapperCache<GUFactory>>()));

class_<sparta::ResourceFactoryBase, boost::noncopyable>
    ("ResourceFactoryBase", no_init);

class_<GUFactory, bases<sparta::ResourceFactoryBase>, boost::noncopyable>
    ("GUResourceFactory", init<>());

class_<sparta::ResourceTreeNode, bases<sparta::TreeNode>, boost::noncopyable>
    ("ResourceTreeNode", no_init)
    .def("__init__", make_constructor(&ResourceTreeNodeWrapper::makeResourceTreeNode))
    .add_property("resource", make_function(+[](sparta::ResourceTreeNode& node){ return node.getResourceNow(); },
                                            return_value_policy<WrapperCache<sparta::Resource>>()));

class_<sparta::PortSet, bases<sparta::TreeNode>, boost::noncopyable>
    ("PortSet", init<sparta::TreeNode*, const std::string&>())
    .add_property("__len__", make_function(+[](sparta::PortSet& p){ return p.getNumChildren(); }));

class_<sparta::EventSet, bases<sparta::TreeNode>, boost::noncopyable>
    ("EventSet", init<sparta::TreeNode*>());

class_<sparta::StatisticSet, bases<sparta::TreeNode>, boost::noncopyable>
    ("StatisticSet", init<sparta::TreeNode*>());

enum_<sparta::Port::Direction>("port_direction")
    .value("IN", sparta::Port::Direction::IN)
    .value("OUT", sparta::Port::Direction::OUT)
    .value("N_DIRECTIONS", sparta::Port::Direction::N_DIRECTIONS);

class_<sparta::Port, bases<sparta::TreeNode>, boost::noncopyable>("Port", no_init);

enum_<sparta::SchedulingPhase>("scheduling_phase")
    .value("TRIGGER", sparta::SchedulingPhase::Trigger)
    .value("UPDATE", sparta::SchedulingPhase::Update)
    .value("PORTUPDATE", sparta::SchedulingPhase::PortUpdate)
    .value("FLUSH", sparta::SchedulingPhase::Flush)
    .value("COLLECTION", sparta::SchedulingPhase::Collection)
    .value("TICK", sparta::SchedulingPhase::Tick)
    .value("POSTTICK", sparta::SchedulingPhase::PostTick)
    .value("LASTSCHEDULINGPHASE", sparta::SchedulingPhase::__last_scheduling_phase)
    .value("INVALID", sparta::SchedulingPhase::Invalid);

class_<sparta::InPort, bases<sparta::Port>, boost::noncopyable>
    ("InPort", no_init); 

class_<sparta::OutPort, bases<sparta::Port>, boost::noncopyable>
    ("OutPort", init<sparta::TreeNode*, const std::string&, bool>());

class_<sparta::DataContainer<bool>, boost::noncopyable>
    ("DataContainer_bool", init<const sparta::Clock*>());

class_<sparta::DataContainer<uint32_t>, boost::noncopyable>
    ("DataContainer_int32", init<const sparta::Clock*>());

class_<sparta::DataContainer<uint64_t>, boost::noncopyable>
    ("DataContainer_int64", init<const sparta::Clock*>());

class_<sparta::DataInPort<bool>, bases<sparta::InPort, sparta::DataContainer<bool>>, boost::noncopyable>
    ("DataInPort_bool", no_init)
    .def("__init__", make_constructor(&PortWrapper<bool>::makeInPort));

class_<sparta::DataInPort<uint32_t>, bases<sparta::InPort, sparta::DataContainer<uint32_t>>, boost::noncopyable>
    ("DataInPort_int32", no_init)
    .def("__init__", make_constructor(&PortWrapper<uint32_t>::makeInPort));

class_<sparta::DataInPort<uint64_t>, bases<sparta::InPort, sparta::DataContainer<uint64_t>>, boost::noncopyable>
    ("DataInPort_int64", no_init)
    .def("__init__", make_constructor(&PortWrapper<uint64_t>::makeInPort));

class_<sparta::DataOutPort<uint32_t>, bases<sparta::OutPort>, boost::noncopyable>
    ("DataOutPort_int32", no_init)
    .def("__init__", make_constructor(&PortWrapper<uint32_t>::makeOutPort));

class_<sparta::DataOutPort<uint64_t>, bases<sparta::OutPort>, boost::noncopyable>
    ("DataOutPort_int64", no_init)
    .def("__init__", make_constructor(&PortWrapper<uint64_t>::makeOutPort));

class_<sparta::InstrumentationNode, bases<sparta::TreeNode>, boost::noncopyable>
    ("InstrumentationNode", init<const std::string&, const std::string&, sparta::InstrumentationNode::Type>());

class_<sparta::CounterBase, bases<sparta::InstrumentationNode>, boost::noncopyable>
    ("CounterBase", no_init);

class_<sparta::StatisticDef::ExpressionArg, boost::noncopyable>
    ("ExpressionArg", init<const std::string&>());

class_<sparta::StatisticDef, bases<sparta::InstrumentationNode>, boost::noncopyable>
    ("StatisticDef", init<sparta::TreeNode*, const std::string&, const std::string&,
                          sparta::TreeNode*, sparta::StatisticDef::ExpressionArg>());

enum_<sparta::CounterBase::CounterBehavior>("counter_behavior")
    .value("COUNT_NORMAL", sparta::CounterBase::CounterBehavior::COUNT_NORMAL)
    .value("COUNT_INTEGRAL", sparta::CounterBase::CounterBehavior::COUNT_INTEGRAL)
    .value("COUNT_LATEST", sparta::CounterBase::CounterBehavior::COUNT_LATEST);

class_<sparta::Counter, bases<sparta::CounterBase>, boost::noncopyable>
    ("Counter", init<sparta::TreeNode*, const std::string&,
                     const std::string&, sparta::CounterBase::CounterBehavior>());

class_<sparta::CycleCounter, bases<sparta::CounterBase>, boost::noncopyable>
    ("CycleCounter", init<sparta::TreeNode*, const std::string&, const std::string&,
                          sparta::CounterBase::CounterBehavior, const sparta::Clock*>());

