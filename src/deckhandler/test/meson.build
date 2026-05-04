foreach t : ['test_shuffle_deal', 'test_cut_strings']
  exe = executable(t, t + '.c', dependencies: deckhandler_dep)
  test(t, exe, suite: 'deckhandler')
endforeach
