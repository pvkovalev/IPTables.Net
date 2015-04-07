﻿using System;
using System.Collections.Generic;
using IPTables.Net.Netfilter;

namespace IPTables.Net.Iptables.Adapter.Client
{
    public interface IIPTablesAdapterClient : INetfilterAdapterClient
    {
        new IpTablesChainSet ListRules(String table);
        void DeleteRule(IpTablesRule rule);
        void InsertRule(IpTablesRule rule);
        void ReplaceRule(IpTablesRule rule);
        void AddRule(IpTablesRule rule);
        Version GetIptablesVersion();
        List<String> GetChains(String table);
    }
}
