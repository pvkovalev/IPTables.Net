﻿using System;
using System.Collections.Generic;
using System.Linq;
using System.Net;
using System.Text;
using IPTables.Net.Iptables.IpSet;
using IPTables.Net.Iptables.IpSet.Adapter;
using IPTables.Net.Tests.MockSystem;
using NUnit.Framework;

namespace IPTables.Net.Tests
{
    [TestFixture]
    class IpSetSyncTests
    {
        [Test]
        public void TestSyncCreate()
        {
            var systemFactory = new MockIpsetSystemFactory();
            var system = new MockIpsetBinaryAdapter(systemFactory);
            var iptables = new IpTablesSystem(systemFactory, null, system);

            IpSetSets rulesOriginal = new IpSetSets(new List<String>()
            {
            }, iptables);

            system.SetSets(rulesOriginal);

            IpSetSets rulesNew = new IpSetSets(new List<String>()
            {
                "create test hash:ip",
                "add test 8.8.8.8"
            }, iptables);

            systemFactory.TestSync(rulesNew, new List<string>
            {
                "create test hash:ip family inet hashsize 1024 maxelem 65536",
                "add test 8.8.8.8"
            });
        }

        [Test]
        public void TestSyncDelete()
        {
            var systemFactory = new MockIpsetSystemFactory();
            var system = new MockIpsetBinaryAdapter(systemFactory);
            var iptables = new IpTablesSystem(systemFactory, null, system);

            IpSetSets rulesOriginal = new IpSetSets(new List<String>()
            {
                "create test hash:ip",
                "add test 8.8.8.8"
            }, iptables);

            system.SetSets(rulesOriginal);

            IpSetSets rulesNew = new IpSetSets(new List<String>()
            {
            }, iptables);

            systemFactory.TestSync(rulesNew, new List<string>
            {
                "destroy test"
            });
        }
    }
}