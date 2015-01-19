#ifndef __CERBERUS_SLOT_MAP_HPP__
#define __CERBERUS_SLOT_MAP_HPP__

#include <map>
#include <set>
#include <algorithm>
#include <functional>

#include "common.hpp"
#include "utils/address.hpp"
#include "utils/random.hpp"

namespace cerb {

    int const CLUSTER_SLOT_COUNT = 16384;

    template <typename Type>
    class SlotMap {
        std::map<slot, util::Address> _slot_range_to_addr;
        std::map<util::Address, Type*> _addr_to_val;
        std::function<Type*(std::string const&, int)> _val_factory;
    public:
        explicit SlotMap(std::function<Type*(std::string const&, int)> vf)
            : _val_factory(std::move(vf))
        {}

        SlotMap(SlotMap const&) = delete;

        bool all_covered() const
        {
            return _slot_range_to_addr.find(CLUSTER_SLOT_COUNT) !=
                _slot_range_to_addr.end();
        }

        template <typename F>
        void iterate_addr(F f) const
        {
            std::for_each(_addr_to_val.begin(), _addr_to_val.end(),
                          [&](std::pair<util::Address, Type*> const& item)
                          {
                              f(item.first);
                          });
        }

        util::Address const& random_addr() const
        {
            while (true) {
                int s = util::randint(0, CLUSTER_SLOT_COUNT);
                auto slot_it = _slot_range_to_addr.upper_bound(s);
                if (slot_it != _slot_range_to_addr.end()) {
                    return slot_it->second;
                }
            }
        }

        Type* get_by_slot(slot s)
        {
            auto slot_it = _slot_range_to_addr.upper_bound(s);
            if (slot_it == _slot_range_to_addr.end()) {
                return nullptr;
            }
            auto val_it = _addr_to_val.find(slot_it->second);
            if (val_it == _addr_to_val.end()) {
                return _addr_to_val[slot_it->second] = _val_factory(
                    slot_it->second.host, slot_it->second.port);
            }
            return val_it->second;
        }

        std::set<Type*> set_map(std::map<slot, util::Address> map)
        {
            std::set<Type*> removed_vals;
            std::map<util::Address, Type*> addr_to_val;
            std::for_each(map.begin(), map.end(),
                          [&](std::pair<slot, util::Address> const& item)
                          {
                              auto val_it = _addr_to_val.find(item.second);
                              if (val_it == _addr_to_val.end()) {
                                  return;
                              }
                              addr_to_val[item.second] = val_it->second;
                              _addr_to_val.erase(val_it);
                          });
            std::for_each(_addr_to_val.begin(), _addr_to_val.end(),
                          [&](std::pair<util::Address, Type*> const& item)
                          {
                              removed_vals.insert(item.second);
                          });
            _addr_to_val = std::move(addr_to_val);
            _slot_range_to_addr = std::move(map);
            return std::move(removed_vals);
        }

        void erase_val(Type* val)
        {
            for (auto i = _addr_to_val.begin(); i != _addr_to_val.end();) {
                if (i->second == val) {
                    i = _addr_to_val.erase(i);
                } else {
                    ++i;
                }
            }
        }
    };

    std::map<slot, util::Address> parse_slot_map(std::string const& nodes_info);
    void write_slot_map_cmd_to(int fd);

}

#endif /* __CERBERUS_SLOT_MAP_HPP__ */
