/*****************************************************************************
 * Copyright (c) 2014-2026 OpenRCT2 developers
 *
 * For a complete list of all authors, please refer to contributors.md
 * Interested in contributing? Visit https://github.com/OpenRCT2/OpenRCT2
 *
 * OpenRCT2 is licensed under the GNU General Public License version 3.
 *****************************************************************************/

// FACTORY-TOUR: fork-owned file.

#pragma once

#include "FactoryRecords.h"

#include <algorithm>
#include <cstdint>
#include <functional>
#include <vector>

namespace OpenRCT2::Factory
{
    /**
     * Dense, id-stable storage for factory records (ADR 0004).
     *
     * Ids are indices. Allocation always returns the lowest free id and iteration is in ascending id
     * order, so two clients that apply the same actions end up with identical pools. Dead slots at the
     * tail are trimmed; dead slots in the middle stay as holes until reused. Persistence writes the
     * dense layout (alive flag per slot) so saved ids match the raw tile payloads.
     */
    template<typename T>
    class Pool
    {
    private:
        std::vector<T> _records;
        std::vector<uint8_t> _alive;
        // Free ids sorted descending so back() is the lowest free id.
        std::vector<RecordId> _freeIds;

    public:
        RecordId allocate()
        {
            RecordId id;
            if (!_freeIds.empty())
            {
                id = _freeIds.back();
                _freeIds.pop_back();
                _records[id] = T{};
                _alive[id] = 1;
            }
            else
            {
                id = static_cast<RecordId>(_records.size());
                _records.emplace_back();
                _alive.push_back(1);
            }
            return id;
        }

        // Allocates and returns the new record; `id` receives its id.
        T& allocateRecord(RecordId& id)
        {
            id = allocate();
            return _records[id];
        }

        void release(RecordId id)
        {
            if (!isAlive(id))
                return;
            _alive[id] = 0;
            _records[id] = T{};
            auto it = std::lower_bound(_freeIds.begin(), _freeIds.end(), id, std::greater<RecordId>());
            _freeIds.insert(it, id);
            trim();
        }

        bool isAlive(RecordId id) const
        {
            return id < _alive.size() && _alive[id] != 0;
        }

        T* get(RecordId id)
        {
            return isAlive(id) ? &_records[id] : nullptr;
        }

        const T* get(RecordId id) const
        {
            return isAlive(id) ? &_records[id] : nullptr;
        }

        size_t aliveCount() const
        {
            return _records.size() - _freeIds.size();
        }

        size_t slotCount() const
        {
            return _records.size();
        }

        void clear()
        {
            _records.clear();
            _alive.clear();
            _freeIds.clear();
        }

        template<typename F>
        void forEach(F f)
        {
            for (size_t i = 0; i < _records.size(); i++)
            {
                if (_alive[i])
                {
                    f(static_cast<RecordId>(i), _records[i]);
                }
            }
        }

        template<typename F>
        void forEach(F f) const
        {
            for (size_t i = 0; i < _records.size(); i++)
            {
                if (_alive[i])
                {
                    f(static_cast<RecordId>(i), _records[i]);
                }
            }
        }

        template<typename V>
        void visit(V& v)
        {
            auto count = static_cast<uint32_t>(_records.size());
            v(count);
            if (v.isReading())
            {
                _records.assign(count, T{});
                _alive.assign(count, 0);
            }
            for (uint32_t i = 0; i < count; i++)
            {
                uint8_t alive = _alive[i];
                v(alive);
                _alive[i] = alive != 0 ? 1 : 0;
                if (_alive[i])
                {
                    _records[i].visit(v);
                }
            }
            if (v.isReading())
            {
                rebuildFreeIds();
                trim();
            }
        }

    private:
        void rebuildFreeIds()
        {
            _freeIds.clear();
            for (size_t i = _alive.size(); i-- > 0;)
            {
                if (!_alive[i])
                {
                    _freeIds.push_back(static_cast<RecordId>(i));
                }
            }
        }

        void trim()
        {
            while (!_alive.empty() && !_alive.back())
            {
                auto id = static_cast<RecordId>(_alive.size() - 1);
                _alive.pop_back();
                _records.pop_back();
                // The highest id is at the front of the descending free list.
                if (!_freeIds.empty() && _freeIds.front() == id)
                {
                    _freeIds.erase(_freeIds.begin());
                }
            }
        }
    };
} // namespace OpenRCT2::Factory
