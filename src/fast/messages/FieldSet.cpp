// Copyright (c) 2009, 2010, 2011 Object Computing, Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
//    * Redistributions of source code must retain the above copyright notice,
//      this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above copyright notice,
//      this list of conditions and the following disclaimer in the documentation
//      and/or other materials provided with the distribution.
//    * Neither the name of Object Computing, Inc. nor the names of its
//      contributors may be used to endorse or promote products derived from this
//      software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE
// FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <Common/QuickFASTPch.h>
#include "FieldSet.h"
#include <Messages/Sequence.h>
#include <Messages/Group.h>
#include <Common/Exceptions.h>
#include <Common/Profiler.h>

using namespace ::QuickFAST;
using namespace ::QuickFAST::Messages;

FieldSet::FieldSet(size_t res)
: fields_(reinterpret_cast<MessageField *>(new unsigned char[sizeof(MessageField) * res]))
, capacity_(res)
, used_(0)
{
  memset(fields_, 0, sizeof(MessageField) * capacity_);
}

FieldSet::~FieldSet()
{
  clear();
  delete [] reinterpret_cast<unsigned char *>(fields_);
}

void
FieldSet::reserve(size_t capacity)
{
  if(capacity > capacity_)
  {
    MessageField * buffer = reinterpret_cast<MessageField *>(new unsigned char[sizeof(MessageField) * capacity]);
    memset(buffer, 0, sizeof(MessageField) * capacity);
    for(size_t nField = 0; nField < used_; ++nField)
    {
      new(&buffer[nField]) MessageField(fields_[nField]);
    }

    MessageField * oldBuffer = fields_;
    size_t oldUsed = used_;
    fields_ = buffer;
    capacity_ = capacity;

    while (oldUsed > 0)
    {
      --oldUsed;
      oldBuffer[oldUsed].~MessageField();
    }
    delete[] reinterpret_cast<unsigned char *>(oldBuffer);
  }
}

void
FieldSet::clear(size_t capacity)
{
  while(used_ > 0)
  {
    --used_;
    fields_[used_].~MessageField();
  }
  if(capacity > capacity_)
  {
    reserve(capacity);
  }
  memset(fields_, 0, sizeof(MessageField) * capacity_);
}

const MessageField &
FieldSet::operator[](size_t index)const
{
  if(index >= used_)
  {
    throw UsageError("Coding Error", "Accessing FieldSet entry: index out of range.");
  }
  return fields_[index];
}

bool
FieldSet::isPresent(const FieldIdentity & identity) const
{
  for(size_t index = 0; index < used_; ++index)
  {
    if(identity == *(fields_[index].getIdentity()))
    {
      return fields_[index].getField()->isDefined();
    }
  }
  return false;
}

void
FieldSet::addField(const FieldIdentityCPtr & identity, const FieldCPtr & value)
{
  PROFILE_POINT("FieldSet::addField");
  if(used_ >= capacity_)
  {
    PROFILE_POINT("FieldSet::grow");
    reserve(((used_ + 1) * 3) / 2);
  }
  new (fields_ + used_) MessageField(identity, value);
  ++used_;
}

bool
FieldSet::getField(const Messages::FieldIdentity & identity, FieldCPtr & value) const
{
  PROFILE_POINT("FieldSet::getField");
  for(size_t index = 0; index < used_; ++index)
  {
    if(identity == *(fields_[index].getIdentity()))
    {
      value = fields_[index].getField();
      return value->isDefined();
    }
  }
  return false;
}

void
FieldSet::getFieldInfo(size_t index, std::string & name, ValueType::Type & type, FieldCPtr & fieldPtr)const
{
  name = fields_[index].name();
  type = fields_[index].getField()->getType();
  fieldPtr = fields_[index].getField();
}

bool
FieldSet::equals (const FieldSet & rhs, std::ostream & reason) const
{
  if(used_ != rhs.used_)
  {
    reason << "Field counts: " << used_ << " != " << rhs.used_;
    return false;
  }
  // application type "any" matches anything.
  if(applicationType_ != "any" && rhs.applicationType_ != "any")
  {
    if(applicationType_ != rhs.applicationType_)
    {
      reason << "Application types: " << applicationType_ << " != " << rhs.applicationType_;
      return false;
    }
    if(!applicationTypeNs_.empty() && !rhs.applicationTypeNs_.empty() && applicationTypeNs_ != rhs.applicationTypeNs_)
    {
      reason << "Application type namespaces: " << applicationTypeNs_ << " != " << rhs.applicationTypeNs_;
      return false;
    }
  }
  for(size_t nField = 0; nField < used_; ++nField)
  {
    if (fields_[nField].name() != rhs.fields_[nField].name())
    {
      reason << "Field[" << nField << "] names: " << fields_[nField].name() << " != " << rhs.fields_[nField].name();
      return false;
    }
    Messages::FieldCPtr f1 = fields_[nField].getField();
    Messages::FieldCPtr f2 = fields_[nField].getField();
    if(*f1 != *f2)
    {
      reason << "Field[" << nField << "] "<< fields_[nField].name() << "values: " << f1->displayString() << " != " << f2->displayString();
      return false;
    }
  }
  return true;
}


bool
FieldSet::getUnsignedInteger(const FieldIdentity & identity, ValueType::Type type, uint64 & value)const
{
  FieldCPtr field;
  bool result = getField(identity, field);
  if(result)
  {
    value = field->toUnsignedInteger();
  }
  return result;
}

bool
FieldSet::getSignedInteger(const FieldIdentity & identity, ValueType::Type type, int64 & value)const
{
  FieldCPtr field;
  bool result = getField(identity, field);
  if(result)
  {
    value = field->toSignedInteger();
  }
  return result;
}

bool
FieldSet::getDecimal(const FieldIdentity & identity,ValueType::Type type, Decimal & value)const
{
  FieldCPtr field;
  bool result = getField(identity, field);
  if(result)
  {
    value = field->toDecimal();
  }
  return result;
}

bool
FieldSet::getString(const FieldIdentity & identity,ValueType::Type type, const StringBuffer *& value)const
{
  FieldCPtr field;
  bool result = getField(identity, field);
  if(result)
  {
    value = & field->toString();
  }

  return result;
}

bool
FieldSet::getGroup(const FieldIdentity & identity, const MessageAccessor *& groupAccessor)const
{
  FieldCPtr field;
  bool result = getField(identity, field);
  if(result)
  {
    const GroupCPtr & group = field->toGroup();
    groupAccessor = group.get();
  }
  return result;
}

void
FieldSet::endGroup(const FieldIdentity & identity, const MessageAccessor * groupAccessor)const
{
}

bool
FieldSet::getSequenceLength(
  const FieldIdentity & identity,
  size_t & length)const
{
  FieldCPtr field;
  bool result = getField(identity, field);
  if(result)
  {
    const SequenceCPtr & sequence = field->toSequence();
    length = sequence->size();
  }
  return result;
}

bool
FieldSet::getSequenceEntry(const FieldIdentity & identity, size_t index, const MessageAccessor *& entryAccessor)const
{
  FieldCPtr field;
  bool result = getField(identity, field);
  if(result)
  {
    const SequenceCPtr & sequence = field->toSequence();
    entryAccessor = (*sequence)[index].get();
  }
  return result;
}

void
FieldSet::endSequenceEntry(const FieldIdentity & identity, size_t index, const MessageAccessor * entryAccessor)const
{
}

void
FieldSet::endSequence(const FieldIdentity & identity)const
{
}
