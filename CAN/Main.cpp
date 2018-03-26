#include "CAN.h"

using namespace CAN;

///////////////////////////////////////////////////////////////////////////////
//                                                                Dumb API Impl
///////////////////////////////////////////////////////////////////////////////

class DumbAllocator final : public Allocator
{
public:
  DumbAllocator(int size) : mSize(size) {}

  AllocatorFactory::RelativeBlockHandle Allocate() override { return reinterpret_cast<uint64_t>(new char[mSize]); }
  void Free(AllocatorFactory::RelativeBlockHandle handle) override { delete[] reinterpret_cast<char*>(handle); }

  void* GetWeakRef(AllocatorFactory::RelativeBlockHandle handle) override { return reinterpret_cast<char*>(handle); }

  AllocatorFactory::AllocatorHandle GetAllocatorHandle() override { return reinterpret_cast<uint64_t>(this); }

private:
  int mSize;
};

class DumbAllocatorFactory : public AllocatorFactory
{
public:
  AllocatorHandle MakeAllocator(size_t size, NodeTypeGUID uid) override { return reinterpret_cast<uint64_t>(new DumbAllocator(size)); }
  void FreeAllocator(Allocator* alloc) override { return delete reinterpret_cast<DumbAllocator*>(alloc); }

  Allocator* LookupAllocator(AllocatorHandle handle) { return reinterpret_cast<Allocator*>(handle); }
};

void MakeTestGraph()
{
  RuntimeManager runtime;
  runtime.RegisterAllocatorFactory(new DumbAllocatorFactory());

  runtime.RegisterStandardLibrary();

  NodeGraph graph(&runtime);

  IntegerLiteralNode* a = runtime.AllocateAndGetWeakRef<IntegerLiteralNode>(3);

  IntegerLiteralNode* b = runtime.AllocateAndGetWeakRef<IntegerLiteralNode>(1);

  IntegerAdditionNode* add1 = runtime.AllocateAndGetWeakRef<IntegerAdditionNode>();
  add1->mA.Connect(&a->mOut);
  add1->mB.Connect(&b->mOut);

  AllocatorFactory::AbsoluteBlockHandle printref;
  IntegerPrinterNode* p1 = runtime.AllocateAndGetWeakRefWithHandle<IntegerPrinterNode>(printref);
  p1->mIn.Connect(&add1->mOut);

  graph.mFinals.push_back(printref);

  graph.Execute();

  NodeForest forest = Forestify(graph);
  forest.Execute();

  std::string cpp = forest.ToCPP();
}

void main()
{
  MakeTestGraph();
  getchar();
}