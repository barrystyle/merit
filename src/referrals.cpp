// Copyright (c) 2017 The Merit Foundation developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include "referrals.h"

#include <utility>

namespace referral
{
ReferralsViewCache::ReferralsViewCache(ReferralsViewDB* db) : m_db{db}
{
    assert(db);
};

MaybeReferral ReferralsViewCache::GetReferral(const Address& address) const
{
    {
        LOCK(m_cs_cache);
        auto it = m_referral_cache.find(address);
        if (it != m_referral_cache.end()) {
            return it->second;
        }
    }

    if (auto ref = m_db->GetReferral(address)) {
        InsertReferralIntoCache(*ref);
        return ref;
    }

    return {};
}

bool ReferralsViewCache::exists(const uint256& hash) const
{
    {
        LOCK(m_cs_cache);
        if (m_referral_hash_cache.count(hash) > 0) {
            return true;
        }
    }

    if (auto ref = m_db->GetReferral(hash)) {
        InsertReferralIntoCache(*ref);
        return true;
    }

    return false;
}

bool ReferralsViewCache::exists(const Address& address) const
{
    {
        LOCK(m_cs_cache);
        if (m_referral_cache.count(address) > 0) {
            return true;
        }
    }
    if (auto ref = m_db->GetReferral(address)) {
        InsertReferralIntoCache(*ref);
        return true;
    }
    return false;
}

void ReferralsViewCache::InsertReferralIntoCache(const Referral& ref) const
{
    LOCK(m_cs_cache);
    //insert into referral cache
    m_referral_cache.insert(std::make_pair(ref.address, ref));
    m_referral_hash_cache.insert(std::make_pair(ref.GetHash(), ref));
}

void ReferralsViewCache::RemoveReferral(const Referral& ref) const
{
    m_referral_cache.erase(ref.address);
    m_referral_hash_cache.erase(ref.GetHash());
    m_db->RemoveReferral(ref);
}

}
