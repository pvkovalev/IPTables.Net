﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using IPTables.Net.Iptables;
using IPTables.Net.Iptables.DataTypes;
using IPTables.Net.Iptables.Modules.Core;
using IPTables.Net.Iptables.RuleGenerator;
using IPTables.Net.TestFramework;
using IPTables.Net.TestFramework.IpTablesRestore;
using NUnit.Framework;

namespace IPTables.Net.Tests
{
    [TestFixture]
    class RuleBuilderSplitterTests
    {
        [Test]
        public void TestSplit()
        {
            var mock = new MockIptablesSystemFactory();
            var system = new IpTablesSystem(mock, new MockIpTablesRestoreAdapter());
            IpTablesChainSet chains = new IpTablesChainSet(4);

            FeatureSplitter<RuleOutputter, IPAddress> ma = new FeatureSplitter<RuleOutputter,IPAddress>("INPUT", "filter", extractor, setter, nestedGenerator, "_");
            ma.AddRule(IpTablesRule.Parse("-A INPUT -s 8.1.1.1 -m udp --sport 1 -j ACCEPT", system, chains));
            ma.AddRule(IpTablesRule.Parse("-A INPUT -s 8.1.1.1 -m udp --sport 2 -j ACCEPT", system, chains));
            ma.AddRule(IpTablesRule.Parse("-A INPUT -s 8.1.1.2 -m udp --sport 3 -j ACCEPT", system, chains));

            IpTablesRuleSet rules = new IpTablesRuleSet(4,system);
            ma.Output(system, rules);

            Assert.AreEqual(3, rules.Chains.Count());
            Assert.AreEqual(2, rules.Chains.First().Rules.Count);
            Assert.AreEqual(2, rules.Chains.Skip(1).First().Rules.Count);
            Assert.AreEqual(1, rules.Chains.Skip(2).First().Rules.Count);
            Assert.AreEqual("-A INPUT -s 8.1.1.1 -j QGkTSfSaLIaS4B/kr3WQ -m comment --comment '_|FS|INPUT_8.1.1.1'",
                rules.Chains.First().Rules.First().GetActionCommand());
            Assert.AreEqual("-A INPUT -s 8.1.1.2 -j ciE0aMcfwN36u0sNiC6w -m comment --comment '_|FS|INPUT_8.1.1.2'",
                rules.Chains.First().Rules.Skip(1).First().GetActionCommand());
            Assert.AreEqual("-A QGkTSfSaLIaS4B/kr3WQ -j ACCEPT -m udp --sport 1",
                rules.Chains.Skip(1).First().Rules.First().GetActionCommand());
        }

        private RuleOutputter nestedGenerator(string arg1, string arg2)
        {
            return new RuleOutputter(arg1, arg2);
        }

        private void setter(IpTablesRule arg1, IPAddress arg2)
        {
            arg1.GetModuleOrLoad<CoreModule>("core").Source = new ValueOrNot<IpCidr>(new IpCidr(arg2));
        }

        private IPAddress extractor(IpTablesRule arg)
        {
            IPAddress addr = arg.GetModule<CoreModule>("core").Source.Value.Address;
            arg.GetModule<CoreModule>("core").Source = new ValueOrNot<IpCidr>();
            return addr;
        }
    }
}
