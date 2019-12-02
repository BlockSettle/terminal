/*

***********************************************************************************
* Copyright (C) 2016 - 2019, BlockSettle AB
* Distributed under the GNU Affero General Public License (AGPL v3)
* See LICENSE or http://www.gnu.org/licenses/agpl.html
*
**********************************************************************************

*/
#ifndef __SETTABLE_FIELD_H__
#define __SETTABLE_FIELD_H__

template<class T>
class SettableField
{
public:
   SettableField() = default;
   explicit SettableField(const T& value)
    : isValid_{true}
    , value_{value}
   {}
   ~SettableField() noexcept = default;

   SettableField(const SettableField<T>&) = default;
   SettableField& operator = (const SettableField<T>&) = default;

   SettableField(SettableField<T>&&) = default;
   SettableField& operator = (SettableField<T>&&) = default;

   bool isValid() const { return isValid_; }

   void setValue(const T& value)
   {
      isValid_ = true;
      value_ = value;
   }

   T getValue() const {
      assert(isValid_);
      return value_;
   }

private:
   bool  isValid_ = false;
   T     value_ = T{};
};

template<class T>
inline bool operator == (const SettableField<T>& l,const SettableField<T>& r)
{
   if (!l.isValid() || !r.isValid()) {
      return false;
   }

   return l.getValue() == r.getValue();
}

#endif // __SETTABLE_FIELD_H__
