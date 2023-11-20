/*  ===========================================================================
*
*   This file is part of HISE.
*   Copyright 2016 Christoph Hart
*
*   HISE is free software: you can redistribute it and/or modify
*   it under the terms of the GNU General Public License as published by
*   the Free Software Foundation, either version 3 of the License, or
*   (at your option) any later version.
*
*   HISE is distributed in the hope that it will be useful,
*   but WITHOUT ANY WARRANTY; without even the implied warranty of
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
*   GNU General Public License for more details.
*
*   You should have received a copy of the GNU General Public License
*   along with HISE.  If not, see <http://www.gnu.org/licenses/>.
*
*   Commercial licenses for using HISE in an closed source project are
*   available on request. Please visit the project's website to get more
*   information about commercial licensing:
*
*   http://www.hise.audio/
*
*   HISE is based on the JUCE library,
*   which must be separately licensed for closed source applications:
*
*   http://www.juce.com
*
*   ===========================================================================
*/

#pragma once

namespace hise {
namespace dispatch {	
using namespace juce;

#if PERFETTO
class PerfettoFlowManager
{
public:

	using PerfettoFlowType = std::function<void(perfetto::EventContext&)>;

	PerfettoFlowManager(RootObject& r, const HashedCharPtr& parent_, const HashedCharPtr& object_):
	  root(r),
	  parent(parent_),
	  object(object_)
	{
		b.ensureAllocated(256);
	}

	void openFlow(DispatchType n, uint8 slotIndex)
	{
		b.clearQuick();
		b << "send " << n << " " << parent << "::" << object << "[" << (int)slotIndex << "]";

		auto& p = pendingFlows.get(n);

		if(p == 0)
			p = root.bumpFlowCounter();
		
		TRACE_EVENT("dispatch", DYNAMIC_STRING_BUILDER(b), perfetto::Flow::ProcessScoped(p));
	}

	PerfettoFlowType closeFlow(DispatchType n)
	{
		//b.clearQuick();
		//b << "flush " << n << " " << parent << "::" << object;

		auto pending = pendingFlows.get(n);

		pendingFlows.get(n) = 0;
		return perfetto::TerminatingFlow::ProcessScoped(pending);
	}

private:

	HashedCharPtr parent, object;
	StringBuilder b;
	DispatchTypeContainer<uint64_t> pendingFlows;
	std::function<uint64_t()> requestFunction;
	RootObject& root;
};
#else
class PerfettoFlowManager
{
public:

	PerfettoFlowManager(RootObject& r, const HashedCharPtr& parent_, const HashedCharPtr& object_) {};

	void openFlow(DispatchType n, uint8 slotIndex) {};
	void closeFlow(DispatchType n) {};
};
#endif

// A object that can send slot change messages to a SourceManager
// Ownership: a member of a subclassed Source object
// Features: 
// - dynamic number of slots
// - a constant sender index (one byte)
// - no heap allocation for <16 byte senders
class SlotSender
{
public:

	SlotSender(Source& s, uint8 index_, const HashedCharPtr& id_);;
	~SlotSender();

	void setNumSlots(uint8 newNumSlots);
	bool flush(DispatchType n=sendNotification);
	bool sendChangeMessage(uint8 indexInSlot, DispatchType notify);

	bool matchesPath(const HashedPath& p, DispatchType n) const
	{
		if(p.slot == id)
		{
			if(p.dispatchType.isWildcard())
				return true;
			else
			{
				// TODO: implement me...
				jassertfalse;
				return false;
			}
		}

		return false;
	}

	Queue* getListenerQueue(DispatchType n) { return &listeners.get(n); }

	void shutdown()
	{
		data.forEach([](SlotBitmap& d) { d.clear(); });
		
		listeners.forEach([](Queue& q) { q.clear(); });
	}

	uint8 getSlotIndex() const noexcept { return index; }

private:

	PerfettoFlowManager flowManager;

	Source& obj;

	const uint8 index;
	const HashedCharPtr id;
	
	DispatchTypeContainer<SlotBitmap> data;
	DispatchTypeContainer<Queue> listeners;

	size_t numSlots;
	
	JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlotSender);
};

} // dispatch
} // hise