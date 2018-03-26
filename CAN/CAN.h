#pragma once

#include <string>
#include <vector>
#include <unordered_map>
#include <iostream>

#include <functional>

namespace CAN
{
  ///////////////////////////////////////////////////////////////////////////////
  //                                                                      Helpers
  ///////////////////////////////////////////////////////////////////////////////

  template<unsigned loop_iterations>
  constexpr unsigned long x65599HashTemplated(const char* str, int str_len, int i)
  {
    return 65599 * x65599HashTemplated<loop_iterations - 1>(str, str_len, i + 1) + str[str_len - i - 1];
  }

  template<>
  constexpr unsigned long x65599HashTemplated< 0 >(const char* str, int str_len, int i)
  {
    return 0;
  }

#define Hash(s) (CAN::x65599HashTemplated<sizeof(s) - 1>(s, sizeof(s) - 1, 0) ^ ((CAN::x65599HashTemplated<sizeof(s) - 1>(s, sizeof(s) - 1, 0)) >> 16))

  ///////////////////////////////////////////////////////////////////////////////
  //                                                               API Structures
  ///////////////////////////////////////////////////////////////////////////////

  using NodeTypeGUID = unsigned long;
  using SlotTypeGUID = unsigned long;

  class Allocator;

  class AllocatorFactory
  {
  public:
    using AllocatorHandle = uint64_t;
    using RelativeBlockHandle = uint64_t;

    struct AbsoluteBlockHandle
    {
      AllocatorHandle mAllocatorHandle;
      RelativeBlockHandle mRelativeBlockHandle;
    };

    virtual ~AllocatorFactory() = default;

    virtual AllocatorHandle MakeAllocator(size_t size, NodeTypeGUID uid) = 0;
    virtual void FreeAllocator(Allocator* alloc) = 0;

    virtual Allocator* LookupAllocator(AllocatorHandle handle) = 0;

    void* GetWeakRef(AbsoluteBlockHandle handle);
  };

  class Allocator
  {
  public:
    virtual ~Allocator() = default;

    virtual AllocatorFactory::RelativeBlockHandle Allocate() = 0;
    virtual void Free(AllocatorFactory::RelativeBlockHandle handle) = 0;

    virtual void* GetWeakRef(AllocatorFactory::RelativeBlockHandle handle) = 0;

    virtual AllocatorFactory::AllocatorHandle GetAllocatorHandle() = 0;

    AllocatorFactory::AbsoluteBlockHandle AbsoluteHandleFromRelativeHandle(AllocatorFactory::RelativeBlockHandle handle);
  };

  ///////////////////////////////////////////////////////////////////////////////
  //                                                          Language Structures
  ///////////////////////////////////////////////////////////////////////////////

  class Node;

  class RuntimeManager
  {
  public:
    template<typename T>
    void RegisterNodeType(std::string str_name, std::vector<std::string> output_names, std::vector<SlotTypeGUID> output_types, std::vector<std::string> input_names, std::vector<SlotTypeGUID> input_types)
    {
      mAllocators[T::TypeGUID] = mFactory->MakeAllocator(sizeof(T), T::TypeGUID);

      mNodeGUIDs.push_back(T::TypeGUID);
      mNodeTypeNames[T::TypeGUID] = str_name;
      mOutputNames[T::TypeGUID] = output_names;
      mOutputTypes[T::TypeGUID] = output_types;
      mInputNames[T::TypeGUID] = input_names;
      mInputTypes[T::TypeGUID] = input_types;

      mInPlaceConstructors[T::TypeGUID] = [](Node* area) { new (area) T(); };
    }

    template<typename NodeType, typename... Args>
    NodeType* AllocateAndGetWeakRefWithHandle(AllocatorFactory::AbsoluteBlockHandle& handleLoc, Args&&... args)
    {
      Allocator* refAllocator = GetAllocator(NodeType::TypeGUID);
      AllocatorFactory::RelativeBlockHandle relativeRefHandle = refAllocator->Allocate(); // @Leak
      NodeType* ref = static_cast<NodeType*>(refAllocator->GetWeakRef(relativeRefHandle));

      new (ref) NodeType(args...);
      ref->mManager = this;

      handleLoc = refAllocator->AbsoluteHandleFromRelativeHandle(relativeRefHandle);

      return ref;
    }

    template<typename NodeType, typename... Args>
    NodeType* AllocateAndGetWeakRef(Args&&... args)
    {
      AllocatorFactory::AbsoluteBlockHandle garbage;
      return AllocateAndGetWeakRefWithHandle<NodeType>(garbage, args...);
    }

    AllocatorFactory::AbsoluteBlockHandle AllocateNode(NodeTypeGUID type, void** args);

    void RegisterAllocatorFactory(AllocatorFactory* factory);
    void RegisterStandardLibrary();

    AllocatorFactory* GetAllocatorFactory() const;
    Allocator* GetAllocator(NodeTypeGUID g) const;

    const std::vector<NodeTypeGUID>& GetNodeGUIDs() const;
    const std::string& GetNodeTypeName(NodeTypeGUID g) const;
    const std::vector<std::string>& GetOutputNames(NodeTypeGUID g) const;
    const std::vector<SlotTypeGUID>& GetOutputTypes(NodeTypeGUID g) const;
    const std::vector<std::string>& GetInputNames(NodeTypeGUID g) const;
    const std::vector<SlotTypeGUID>& GetInputTypes(NodeTypeGUID g) const;

  private:
    // Runtime Data
    AllocatorFactory * mFactory = nullptr;
    std::unordered_map<NodeTypeGUID, AllocatorFactory::AllocatorHandle> mAllocators;

    // Introspection Data
    std::vector<NodeTypeGUID> mNodeGUIDs;
    std::unordered_map<NodeTypeGUID, std::string> mNodeTypeNames;
    std::unordered_map<NodeTypeGUID, std::vector<std::string>> mOutputNames;
    std::unordered_map<NodeTypeGUID, std::vector<SlotTypeGUID>> mOutputTypes;
    std::unordered_map<NodeTypeGUID, std::vector<std::string>> mInputNames;
    std::unordered_map<NodeTypeGUID, std::vector<SlotTypeGUID>> mInputTypes;
    std::unordered_map<NodeTypeGUID, std::function<void(Node*)>> mInPlaceConstructors;
  };

  ///////////////////////////////////////////////////////////////////////////////
  //                                                             Graph Structures
  ///////////////////////////////////////////////////////////////////////////////

  class Node;
  class Slot;

  class SlotConnection
  {
  public:
    Slot * mOutput;
    Slot* mInput;
  };

#define SlotMixin(TYPE) \
static const CAN::SlotTypeGUID TypeGUID = Hash(#TYPE); \
CAN::SlotTypeGUID GetTypeGUID() override { return TypeGUID; }

  class Slot
  {
  public:
    Slot(Node* parent);
    virtual ~Slot() = default;

    virtual bool CanConnect(Slot* other);
    void Connect(Slot* other);

    virtual SlotTypeGUID GetTypeGUID() = 0;

    Node* mParent;
    std::vector<SlotConnection> mConnectedTo;
  };

#define NodeMixin(TYPE) \
static const CAN::NodeTypeGUID TypeGUID = Hash(#TYPE); \
CAN::NodeTypeGUID GetTypeGUID() override { return TypeGUID; }

  class Node
  {
  public:
    virtual ~Node() = default;
    virtual std::vector<Slot*> GetInputs();
    virtual std::vector<Slot*> GetOutputs();

    virtual void Populate(void** args) {}

    virtual void Execute();
    virtual std::string ToCPP();

    virtual NodeTypeGUID GetTypeGUID() = 0;

    RuntimeManager* GetRuntime();

  private:
    RuntimeManager * mManager;

    friend RuntimeManager;
  };

  // Does unessisary execution
  class NodeGraph
  {
  public:
    NodeGraph(RuntimeManager* manager);

    void Execute();
    RuntimeManager* GetRuntime() const;

    std::vector<AllocatorFactory::AbsoluteBlockHandle> mFinals;
  private:
    RuntimeManager * mManager;
  };

  // Uses local variables to avoid unessisary execution and can
  // be transpiled to C++
  class NodeForest
  {
  public:
    NodeForest(RuntimeManager* manager);

    void Execute();
    std::string ToCPP();
    RuntimeManager* GetRuntime() const;

    std::vector<AllocatorFactory::AbsoluteBlockHandle> mRoots;

  private:
    RuntimeManager * mManager;
  };

  ///////////////////////////////////////////////////////////////////////////////
  //                                                             Standard Library
  ///////////////////////////////////////////////////////////////////////////////
  // Keeping this all in header because the only time the functions should change
  // is if the data layout changes, which would inccur a rebuild anyways.
  //
  ///////////////////////////////////////////////////////////////////////////////

  std::string GenerateRandomVarName();

  class IntegerSlot : public Slot
  {
  public:
    SlotMixin(IntegerSlot);

    IntegerSlot(Node* parent) : Slot(parent), mValue(0)
    {
    }

    int mValue;
  };

  class IntegerLiteralNode : public Node
  {
  public:
    NodeMixin(IntegerLiteralNode);

    IntegerLiteralNode() : mOut(this) {}

    IntegerLiteralNode(int value) : IntegerLiteralNode()
    {
      mOut.mValue = value;
      mValue = value;
    }

    std::vector<Slot*> GetInputs() override
    {
      return {};
    }

    std::vector<Slot*> GetOutputs() override
    {
      return { &mOut };
    }

    void Execute() override {}

    std::string ToCPP() override
    {
      return std::to_string(mValue);
    }

    void Populate(void** args) override
    {
      mValue = *static_cast<int*>(args[0]);
    }

    IntegerSlot mOut;
    int mValue;
  };

  class IntegerAdditionNode : public Node
  {
  public:
    NodeMixin(IntegerAdditionNode);

    IntegerAdditionNode() : mA(this), mB(this), mOut(this)
    {
    }

    virtual std::vector<Slot*> GetInputs()
    {
      return { &mA, &mB };
    }

    virtual std::vector<Slot*> GetOutputs()
    {
      return { &mOut };
    }

    void Execute() override
    {
      mA.mConnectedTo[0].mOutput->mParent->Execute();
      mB.mConnectedTo[0].mOutput->mParent->Execute();

      mOut.mValue = static_cast<IntegerSlot*>(mA.mConnectedTo[0].mOutput)->mValue + static_cast<IntegerSlot*>(mB.mConnectedTo[0].mOutput)->mValue;
    }

    std::string ToCPP() override
    {
      std::string left = mA.mConnectedTo[0].mOutput->mParent->ToCPP();
      std::string right = mB.mConnectedTo[0].mOutput->mParent->ToCPP();

      return left + "+" + right;
    }

    IntegerSlot mA;
    IntegerSlot mB;

    IntegerSlot mOut;
  };

  class IntegerPrinterNode : public Node
  {
  public:
    NodeMixin(IntegerPrinterNode);

    IntegerPrinterNode() : mIn(this)
    {
    }

    virtual std::vector<Slot*> GetInputs()
    {
      return { &mIn };
    }

    virtual std::vector<Slot*> GetOutputs()
    {
      return {};
    }

    void Execute() override
    {
      IntegerSlot* s = static_cast<IntegerSlot*>(mIn.mConnectedTo[0].mOutput);
      s->mParent->Execute();

      std::cout << s->mValue << std::endl;
    }

    std::string ToCPP() override
    {
      std::string internal = mIn.mConnectedTo[0].mOutput->mParent->ToCPP();
      return "printf(\"%i\", " + internal + ")";
    }

    IntegerSlot mIn;
  };

  class StoreIntegerVariableNode : public Node
  {
  public:
    NodeMixin(StoreIntegerVariableNode);

    StoreIntegerVariableNode() : mIn(this)
    {
    }

    virtual std::vector<Slot*> GetInputs()
    {
      return { &mIn };
    }

    virtual std::vector<Slot*> GetOutputs()
    {
      return {};
    }

    void Execute() override
    {
      mIn.mConnectedTo[0].mOutput->mParent->Execute();

      mValue = static_cast<IntegerSlot*>(mIn.mConnectedTo[0].mOutput)->mValue;
    }

    std::string ToCPP() override
    {
      std::string internal = mIn.mConnectedTo[0].mOutput->mParent->ToCPP();
      mName = GenerateRandomVarName();

      return "int " + mName + " = " + internal;
    }

    std::string mName;
    int mValue;

    IntegerSlot mIn;
  };

  class LoadIntegerVariableNode : public Node
  {
  public:
    NodeMixin(LoadIntegerVariableNode);

    LoadIntegerVariableNode() : mOut(this)
    {
    }

    LoadIntegerVariableNode(AllocatorFactory::AbsoluteBlockHandle storeIntegerVariableNodeHandle) : LoadIntegerVariableNode()
    {
      mPair = storeIntegerVariableNodeHandle;
    }

    virtual std::vector<Slot*> GetInputs()
    {
      return {};
    }

    virtual std::vector<Slot*> GetOutputs()
    {
      return { &mOut };
    }

    void Execute() override
    {
      mOut.mValue = static_cast<StoreIntegerVariableNode*>(GetRuntime()->GetAllocatorFactory()->GetWeakRef(mPair))->mValue;
    }

    std::string ToCPP() override
    {
      return static_cast<StoreIntegerVariableNode*>(GetRuntime()->GetAllocatorFactory()->GetWeakRef(mPair))->mName;
    }

    IntegerSlot mOut;

  private:
    AllocatorFactory::AbsoluteBlockHandle mPair;
  };

  ///////////////////////////////////////////////////////////////////////////////
  //                                                                Graph Helpers
  ///////////////////////////////////////////////////////////////////////////////

  // Will ruin cluster FYI
  NodeGraph NodeClusterToNodeGraph(RuntimeManager* runtime, std::vector<AllocatorFactory::AbsoluteBlockHandle>& nodes);

  void RecursiveOutputCheck(Node* node, std::vector<AllocatorFactory::AbsoluteBlockHandle>& roots);

  // Will ruin graph FYI
  NodeForest Forestify(NodeGraph ng);
}
