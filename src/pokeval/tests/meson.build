cases = [
  'test',
  'bringin',
  'compare_hands',
  'lowball',
  'nothing',
  'omaha',
  'pair',
  'straight',
  'sort_hand',
  '6-card-hand',
  '7-card-hand',
  'four-of-a-kind',
  'five-of-a-kind',
  'wild',
  'flush',
  'score_stud_upcards',
  'no_peek_visible',
]

foreach t : cases
  exe = executable(
    'test_' + t,
    [t + '.c', '00_test.c'],
    dependencies: pokeval_dep,
    override_options: ['b_lto=false'],
  )
  test('test_' + t, exe, suite: 'pokeval')
endforeach
