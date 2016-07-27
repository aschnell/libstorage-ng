#!/usr/bin/python

# storage integration test utils

# TODO can this be made available as storage.itu?

from storage import *
from os import system

class MyCommitCallbacks(CommitCallbacks):

    def __init__(self):
        super(MyCommitCallbacks, self).__init__()

    def message(self, message):
        print "message '%s'" % message

    def error(self, message, what):
        print "error '%s' '%s'" % (message, what)
        return False


def commit(storage, save_graphs = False):

    if save_graphs:
        storage.get_probed().write_graphviz("probed.gv", GraphvizFlags_CLASSNAME |
			                    GraphvizFlags_SID | GraphvizFlags_SIZE);
        system("dot -Tpng < probed.gv > probed.png")

        storage.get_staging().write_graphviz("staging.gv", GraphvizFlags_CLASSNAME |
			                     GraphvizFlags_SID | GraphvizFlags_SIZE);
        system("dot -Tpng < staging.gv > staging.png")

    my_commit_callbacks = MyCommitCallbacks()

    actiongraph = storage.calculate_actiongraph()

    if save_graphs:
        actiongraph.write_graphviz("action.gv", GraphvizFlags_SID)
        system("dot -Tpng < action.gv > action.png")

    storage.commit(my_commit_callbacks)

