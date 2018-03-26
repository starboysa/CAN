#include "CAN.h"
#include <iostream>
#include <memory>

using namespace CAN;

///////////////////////////////////////////////////////////////////////////////
//                                                               API Structures
///////////////////////////////////////////////////////////////////////////////

AllocatorFactory::AbsoluteBlockHandle Allocator::AbsoluteHandleFromRelativeHandle(AllocatorFactory::RelativeBlockHandle handle)
{
  return { GetAllocatorHandle(), handle };
}

void* AllocatorFactory::GetWeakRef(AbsoluteBlockHandle handle)
{
  return LookupAllocator(handle.mAllocatorHandle)->GetWeakRef(handle.mRelativeBlockHandle);
}

///////////////////////////////////////////////////////////////////////////////
//                                                          Language Structures
///////////////////////////////////////////////////////////////////////////////

#include <functional>



AllocatorFactory::AbsoluteBlockHandle RuntimeManager::AllocateNode(NodeTypeGUID type, void** args)
{
  auto allocator = mFactory->LookupAllocator(mAllocators[type]);
  
  auto relativeBlock = allocator->Allocate();
  Node* n = static_cast<Node*>(allocator->GetWeakRef(relativeBlock));

  mInPlaceConstructors[type](n);
  n->mManager = this;

  auto block = allocator->AbsoluteHandleFromRelativeHandle(relativeBlock);
  n->Populate(args);

  return block;
}

void RuntimeManager::RegisterAllocatorFactory(AllocatorFactory* factory)
{
  mFactory = factory;
}

AllocatorFactory* RuntimeManager::GetAllocatorFactory() const { return mFactory; }
Allocator* RuntimeManager::GetAllocator(NodeTypeGUID g) const { return mFactory->LookupAllocator(mAllocators.at(g)); }

const std::vector<NodeTypeGUID>& RuntimeManager::GetNodeGUIDs() const { return mNodeGUIDs; }
const std::string& RuntimeManager::GetNodeTypeName(NodeTypeGUID g) const { return mNodeTypeNames.at(g); }
const std::vector<std::string>& RuntimeManager::GetOutputNames(NodeTypeGUID g) const { return mOutputNames.at(g); }
const std::vector<SlotTypeGUID>& RuntimeManager::GetOutputTypes(NodeTypeGUID g) const { return mOutputTypes.at(g); }
const std::vector<std::string>& RuntimeManager::GetInputNames(NodeTypeGUID g) const { return mInputNames.at(g); }
const std::vector<SlotTypeGUID>& RuntimeManager::GetInputTypes(NodeTypeGUID g) const { return mInputTypes.at(g); }

///////////////////////////////////////////////////////////////////////////////
//                                                             Graph Structures
///////////////////////////////////////////////////////////////////////////////

Slot::Slot(Node* parent) : mParent(parent) {}

bool Slot::CanConnect(Slot* other)
{
  return GetTypeGUID() == other->GetTypeGUID();
}

void Slot::Connect(Slot* other)
{
  SlotConnection connection;
  connection.mInput = this;
  connection.mOutput = other;

  mConnectedTo.push_back(connection);
  other->mConnectedTo.push_back(connection);
}

std::vector<Slot*> Node::GetInputs() { return {}; }
std::vector<Slot*> Node::GetOutputs() { return {}; }

void Node::Execute() { printf("Base node executed.\n"); }
std::string Node::ToCPP() { return "ERROR: Base Node Printed"; }

RuntimeManager* Node::GetRuntime() { return mManager; }

///////////////////////////////////////////////////////////////////////////////
//                                                                Graph Helpers
///////////////////////////////////////////////////////////////////////////////

NodeGraph::NodeGraph(RuntimeManager* manager) : mManager(manager) {}

void NodeGraph::Execute()
{
  for (AllocatorFactory::AbsoluteBlockHandle node : mFinals)
  {
    static_cast<Node*>(mManager->GetAllocatorFactory()->GetWeakRef(node))->Execute();
  }
}

RuntimeManager* NodeGraph::GetRuntime() const { return mManager; }

NodeForest::NodeForest(RuntimeManager* manager) : mManager(manager) {}

void NodeForest::Execute()
{
  for (AllocatorFactory::AbsoluteBlockHandle node : mRoots)
  {
    static_cast<Node*>(mManager->GetAllocatorFactory()->GetWeakRef(node))->Execute();
  }
}

std::string NodeForest::ToCPP()
{
  std::string acc = "{\n";
  for (AllocatorFactory::AbsoluteBlockHandle node : mRoots)
  {
    acc += static_cast<Node*>(mManager->GetAllocatorFactory()->GetWeakRef(node))->ToCPP() + ";\n";
  }

  acc += "}\n";
  return acc;
}

RuntimeManager* NodeForest::GetRuntime() const { return mManager; }

///////////////////////////////////////////////////////////////////////////////
//                              Graph Helpers That Require The Standard Library
///////////////////////////////////////////////////////////////////////////////

void RuntimeManager::RegisterStandardLibrary()
{
  RegisterNodeType<IntegerLiteralNode>("IntegerLiteralNode", { "mOut" }, { IntegerSlot::TypeGUID }, {}, {});
  RegisterNodeType<IntegerAdditionNode>("IntegerAdditionNode", { "mOut" }, { IntegerSlot::TypeGUID }, { "a", "b" }, { IntegerSlot::TypeGUID, IntegerSlot::TypeGUID });
  RegisterNodeType<IntegerPrinterNode>("IntegerPrinterNode", {}, {}, { "mIn" }, { IntegerSlot::TypeGUID });

  RegisterNodeType<StoreIntegerVariableNode>("StoreIntegerVariableNode", {}, {}, { "mIn" }, { IntegerSlot::TypeGUID });
  RegisterNodeType<LoadIntegerVariableNode>("LoadIntegerVariableNode", { "mOut" }, { IntegerSlot::TypeGUID }, {}, {});
}

std::string CAN::GenerateRandomVarName()
{
  std::string str;
  static char singleLetterName = 'a';
  str.push_back(singleLetterName);
  ++singleLetterName;
  return str;
}

NodeGraph CAN::NodeClusterToNodeGraph(RuntimeManager* runtime, std::vector<AllocatorFactory::AbsoluteBlockHandle>& nodes)
{
  NodeGraph graph(runtime);

  // Identify Roots
  for(auto& block : nodes)
  {
    Node* node = static_cast<Node*>(runtime->GetAllocatorFactory()->GetWeakRef(block));

    if(node->GetOutputs().empty())
    {
      graph.mFinals.push_back(block);
    }
  }

  return graph;
  // @ErrorChecking(Jacob): Add some way to check for floating clusters
}

void CAN::RecursiveOutputCheck(Node* node, std::vector<AllocatorFactory::AbsoluteBlockHandle>& roots)
{
  // For loop takes care of our base case, if the node doesn't have any inputs the for loop will never run 
  // and the recursion will terminate
  for(Slot* input : node->GetInputs())
  {
    Slot* output = input->mConnectedTo[0].mOutput; // Should only be one connection per input

    // If the output is connected to multiple inputs
    if (output->mConnectedTo.size() > 1)
    {
      // Create a temporary and hook that up to the input instead
      
      AllocatorFactory::AbsoluteBlockHandle store;
      StoreIntegerVariableNode* storeArea = node->GetRuntime()->AllocateAndGetWeakRefWithHandle<StoreIntegerVariableNode>(store);

      // Connect all inputs from the multi-output to a new load
      for(SlotConnection& connection : output->mConnectedTo)
      {
        LoadIntegerVariableNode* load = node->GetRuntime()->AllocateAndGetWeakRef<LoadIntegerVariableNode>(store);

        connection.mInput->mConnectedTo.clear();
        connection.mInput->Connect(&load->mOut);
      }

      roots.push_back(store);

      // Connect the output to the store
      output->mConnectedTo.clear();

      storeArea->mIn.Connect(output);
    }

    // Recurse
    RecursiveOutputCheck(output->mParent, roots);
  }
}

// Will ruin graph FYI
NodeForest CAN::Forestify(NodeGraph ng)
{
  NodeForest forest(ng.GetRuntime());
  std::vector<AllocatorFactory::AbsoluteBlockHandle> addedNodes;

  for(AllocatorFactory::AbsoluteBlockHandle final : ng.mFinals)
  {
    Node* node = static_cast<Node*>(forest.GetRuntime()->GetAllocatorFactory()->GetWeakRef(final));

    RecursiveOutputCheck(node, addedNodes);
  }

  for(auto itr = addedNodes.rbegin(); itr != addedNodes.rend(); ++itr)
  {
    forest.mRoots.push_back(*itr);
  }

  for (AllocatorFactory::AbsoluteBlockHandle final : ng.mFinals)
  {
    forest.mRoots.push_back(final);
  }
  
  return forest;
}


///////////////////////////////////////////////////////////////////////////////
// Todo:
//   Introspection
//   Editor Integration
//   Component Representation
//
//-----------------------------------------------------------------------------
//
//   API
//     Instruction Memory Mangement
//     Data Memory Management
//     Allocation and Threading Callbacks
//   Standard Library
//   all Node* (mParent) need to be AbsoluteBlocks
//
///////////////////////////////////////////////////////////////////////////////
// Design Notes:
//
// MEMORY MANAGEMENT AND OTHER THINGS I DON'T WANNA DO
//
// Memory managment is split into two major sections:
// Instruction Memory Management
// Data Memory Management
//
// Instruction memory we don't really care too much about because
// ideally it gets handled by the C++ runtime once it's transpiled
// so the debug rep can just use shitty shared ptrs.
//
// Data Memory is much more important as it's important to keep it
// fast for cache reasons.  So instead of doing it myself I'd like
// to push that responsibility to the users.  How?  Well we can
// steal a lot from OpenGL.  OpenGL has two clients, Software Devs
// and Graphics Card Devs.  Software Devs send in data to be operated
// on and the Graphics Card provides the functionality including allocation
// and threading.  We can make use of this design, but we only have one
// client, the user.  So we have to give the user the ability to send
// data through the pipeline as well as customize the pipeline to their
// likeing.  I don't think it's necissary to provide the same programmable
// customization that OpenGL does, but there are a specific set of functions
// that I'd like to use client callbacks for:
//         Allocation
//         StartThread
// And the way these work is going to be different than OpenGL because
// the client that's asking for the allocation is also providing
// the allocation function.
//
// INTROSPECTION
//
// Need some way to loop through the members on a component and get/set them
// for Editor and Serialization purposes.  We could also intrinsically support
// some sort of versioning helper, but it'd be better if we could somehow
// make that generic rather than a feature of the language.  That being said,
// it'd be good to have even if it does end up being a language feature.
//
///////////////////////////////////////////////////////////////////////////////
