// Types for the dasmig persona generator. The JSON shapes are those of the
// C++ libraries' to_json(); see each library's doc/usage.md.

/** The family to draw around a persona. Every field is optional. */
export interface RelationsRequest {
  /** Generations above the persona: 0 none, 1 parents (default), 2 grandparents too. */
  up?: 0 | 1 | 2;
  /** Generations below the persona: 0 none, 1 children (default), 2 grandchildren too. */
  down?: 0 | 1 | 2;
  /** Include siblings and half-siblings; default true. They come from the parents, so `up: 0` gives none. */
  siblings?: boolean;
  /** Include partners, current and former; default true. */
  partners?: boolean;
  /** Give everyone a name; default true. */
  names?: boolean;
  /** Add the partners a parent took while the persona was a child, as `stepfather` and `stepmother`; default false. Needs `up: 1` or more. With `siblings`, also their children: those with the parent are `half_sibling`s, those from before are `stepsibling`s. */
  stepparents?: boolean;
  /** Add `household`: who the persona lives with at the reference date; default false. */
  home?: boolean;
}

export type Relation =
  | 'anchor' | 'father' | 'mother' | 'sibling' | 'half_sibling'
  | 'paternal_grandfather' | 'paternal_grandmother' | 'maternal_grandfather' | 'maternal_grandmother'
  | 'partner' | 'child' | 'grandchild' | 'stepchild' | 'stepfather' | 'stepmother' | 'stepsibling';

export interface FamilyMember {
  /** Index in `members`; the edges refer to it. */
  id: number;
  role: Relation;
  sex: Sex;
  /** Absent when names are off or the country's culture has no names. */
  name?: { given: string; patronymic?: string; family: string; full: string };
  /** YYYY-MM-DD. */
  birth: string;
  twin?: true;
  alive: boolean;
  /** Present when alive. */
  age?: number;
  /** Present when dead. */
  death_age?: number;
  /** YYYY-MM-DD; present when dead. */
  death?: string;
}

export type FamilyEdge =
  | { kind: 'parent_of'; from: number; to: number; genetic?: false }
  | { kind: 'sibling_of'; from: number; to: number; full: boolean }
  | { kind: 'partner_of'; from: number; to: number; start: number; end?: number; ended: 'ongoing' | 'divorce' | 'death' }
  /** `from` is a stepfather or stepmother of `to` (the persona). */
  | { kind: 'step_parent_of'; from: number; to: number };

export type HouseholdType =
  | 'one_person' | 'couple' | 'nuclear' | 'single_parent' | 'with_parents' | 'multi_generation' | 'other';

/** Who the persona lives with at the reference date; present only when `relations.home` was set. */
export interface Household {
  type: HouseholdType;
  /** Member ids, the persona (0) first; all alive. */
  members: number[];
}

/** The persona's family, as relationship-generator writes it. `members[0]` is the persona. */
export interface Family {
  country: string;
  /** YYYY-MM-DD: the date ages and who is alive refer to. */
  reference_date: string;
  members: FamilyMember[];
  edges: FamilyEdge[];
  household?: Household;
  sources: { fertility: string; unions: string };
  signature: Signature;
}

/** What a persona call asks for. Every field is optional: what is left out is drawn. */
export interface PersonaRequest {
  /**
   * ISO 3166-1 alpha-2 code, any case, from the generator's `countries`; another code is a RangeError.
   * Unset draws one of `countries`, weighted by population.
   */
  country?: string;
  /** Unset draws it with the birth. */
  sex?: Sex;
  /** Inclusive age range in completed years (0-120). Unset draws from the country's age pyramid. */
  ages?: { min: number; max: number };
  /**
   * Number of personality traits, 0 to the generator's `maxTraits`; default 3. Fewer are returned
   * when no other trait is compatible with those already chosen (traits in one group exclude each other).
   */
  traits?: number;
  /**
   * The date ages are computed at: `YYYY-MM-DD`, or a Date (its local calendar date).
   * Default 2023-07-01. The generator never reads the clock, so pass today's date for ages "as of now".
   */
  referenceDate?: string | Date;
  /**
   * Also draw the persona's family (`relatives` in the result). `true` for the defaults, or an object.
   * Unset draws none, and then nothing about the persona changes. The family has its own seed stream,
   * so the rest of the persona is the same with and without it.
   */
  relations?: true | RelationsRequest;
  /**
   * Add `identity`, the persona's national numbers and documents (id-generator), from their birth date,
   * sex, name and city. `true` asks for the numbers only. Unset, nothing about the persona changes.
   */
  identity?: boolean | { documents?: ('passport' | 'id_card')[] };
  /**
   * Add `finance`, the persona's bank accounts and payment cards (finance-generator). `true` asks for one
   * current account. What is asked for is a chance, by the country's Global Findex shares, so many
   * have none. Unset, nothing about the persona changes. A card number from `network_range`, the
   * default without `safe`, may match a real card: test data only.
   */
  finance?: boolean | {
    accounts?: ('current' | 'savings')[];
    cards?: ('debit' | 'credit' | 'prepaid' | 'any')[];
    cardNumbers?: 'network_range' | 'test_number' | 'wrong_check';
    network?: string;
  };
  /** Make the numbers, documents, accounts and cards by the libraries' safe methods. Default false. */
  safe?: boolean;
  /**
   * Add `home`, the persona's home (property-generator): its dwelling type, tenure, rooms, floor area,
   * year built and monthly cost, and `household_income`, what everyone in the household earns. `true`
   * draws everything; an object forces the type, the tenure or the area. The household is the family's
   * when `relations` is asked (and its `home` is then on), otherwise one drawn for the home alone.
   * Unset, nothing about the persona changes.
   */
  home?: boolean | {
    type?: DwellingType;
    tenure?: Tenure;
    area?: 'city' | 'town' | 'rural';
  };
  /**
   * Add `employer`, the company the persona works for (company-generator): its name, industry, size,
   * legal form, founding date, registration numbers and domain. A business is drawn by its staff, in
   * the persona's country, in an industry that employs the persona's occupation. A persona who is not
   * employed has none. With `safe`, its numbers and domain are safe too. Unset, nothing about the
   * persona changes.
   */
  employer?: boolean;
  /**
   * The call seed, a 64-bit unsigned integer: a bigint, a non-negative safe integer, or a string
   * of 1-16 **hexadecimal** digits with an optional `0x` (so `'42'` is 0x42, not 42), the form of
   * `signature.seed`. Unset draws a random one. The same seed, request and data give the same persona.
   */
  seed?: string | bigint | number;
}

export type Sex = 'male' | 'female';

/** The seed a result was generated from and a hash of the data it used (16 hex digits each). */
export interface Signature {
  seed: string;
  data: string;
}

export interface Country {
  cca2: string;
  cca3: string;
  ccn3: string;
  name_common: string;
  name_official: string;
  capital: string[];
  region: string;
  subregion: string;
  continent: string[];
  latitude: number;
  longitude: number;
  area: number;
  population: number;
  landlocked: boolean;
  independent: boolean;
  un_member: boolean;
  languages: string[];
  currency_code: string;
  currency_name: string;
  currency_symbol: string;
  borders: string[];
  timezones: string[];
  driving_side: string;
  tld: string;
  idd_root: string;
  idd_suffix: string;
  demonym_m: string;
  demonym_f: string;
  flag_emoji: string;
  income_level: string;
  start_of_week: string;
  signature: Signature;
}

export interface City {
  geonameid: number;
  name: string;
  asciiname: string;
  latitude: number;
  longitude: number;
  feature_code: string;
  country_code: string;
  cc2: string;
  admin1_code: string;
  admin2_code: string;
  admin3_code: string;
  admin4_code: string;
  population: number;
  /** Metres; null when unknown. */
  elevation: number | null;
  /** Digital elevation model, metres. */
  dem: number;
  timezone: string;
  signature: Signature;
}

export interface Birth {
  country_code: string;
  /** YYYY-MM-DD. */
  date: string;
  year: number;
  month: number;
  day: number;
  /** Completed years at the reference date. */
  age: number;
  sex: Sex;
  /** 0 = Sunday. */
  weekday: number;
  /** Remaining life expectancy at that age, years (a population model, not a forecast). */
  le_remaining: number;
  cohort: string;
  signature: Signature;
}

/** `intermediate` covers green, hazel and amber. */
export type EyeColor = 'blue' | 'intermediate' | 'brown';
/** `grey` only at older ages. */
export type HairColor = 'black' | 'brown' | 'blond' | 'red' | 'grey';
/** Fitzpatrick skin phototype. */
export type SkinType = 'I' | 'II' | 'III' | 'IV' | 'V' | 'VI';
export type BloodType = 'O+' | 'O-' | 'A+' | 'A-' | 'B+' | 'B-' | 'AB+' | 'AB-';
export type Handedness = 'right' | 'left';

export interface Biodata {
  country: string;
  sex: Sex;
  height_cm: number;
  weight_kg: number;
  bmi: number;
  eyes: EyeColor;
  hair: HairColor;
  skin: SkinType;
  blood: BloodType;
  hand: Handedness;
  age: number;
  signature: Signature;
}

export interface Trait {
  key: string;
  label: string;
  /** The exclusive group the trait belongs to, if any. */
  group: string | null;
  tags: string[];
}

export interface Personality {
  traits: Trait[];
  /** Big Five scores: centered on zero, unbounded model values, not percentiles. */
  axes: {
    openness: number;
    conscientiousness: number;
    extraversion: number;
    agreeableness: number;
    neuroticism: number;
  };
  signature: Signature;
}

export interface InterestItem {
  key: string;
  label: string;
}

export interface Interest {
  country: string;
  religion: string;
  religion_group: string;
  hobbies: InterestItem[];
  films: InterestItem[];
  games: InterestItem[];
  books: InterestItem[];
  music: InterestItem[];
  signature: Signature;
}

export type EmploymentStatus = 'employed' | 'unemployed' | 'student' | 'retired' | 'inactive';
export type EducationLevel = 'less_than_basic' | 'basic' | 'intermediate' | 'advanced';

export interface Job {
  key: string;
  label: string;
  /** ISCO-08 major or sub-major group code. */
  isco: string;
  isco_label: string;
  /** ISCO-08 unit group code. */
  unit: string;
  /** ISCO-08 skill level, "1" to "4". */
  skill: string;
  skills: string[];
  gear: string[];
  tags: string[];
}

/** An amount of money, in major units of an ISO 4217 currency. */
export interface Money {
  amount: number;
  /** ISO 4217 alphabetic code, such as "BRL". */
  currency: string;
}

export interface Occupation {
  country: string;
  sex: Sex;
  age: number;
  status: EmploymentStatus;
  education: EducationLevel | null;
  /** Null unless employed. */
  job: Job | null;
  /** Gross annual income in the country's currency; null unless employed, or where the country is not served. */
  income: Money | null;
  signature: Signature;
}

/** property-generator's dwelling types. */
export type DwellingType = 'detached' | 'semi_detached' | 'flat_small' | 'flat_large' | 'informal' | 'other';
/** property-generator's tenures. */
export type Tenure = 'owner_outright' | 'owner_mortgage' | 'tenant_market' | 'tenant_reduced';

/** The home, as property-generator writes it. */
export interface Home {
  country_code: string;
  area: 'city' | 'town' | 'rural';
  household_type: 'one_person' | 'couple' | 'nuclear' | 'single_parent' | 'with_parents' | 'multi_generation' | 'other';
  household_size: number;
  type: DwellingType;
  holding: Tenure;
  /** Habitable rooms, the kitchen counted, bathrooms not. */
  rooms: number;
  bedrooms: number;
  /** Square metres. */
  floor_area: number;
  year_built: number;
  /** Rent, or the owner's mortgage and charges, a month; null without a household income. */
  monthly_cost: Money | null;
  /** The weakest evidence of the fields drawn. */
  evidence: 'measured' | 'modelled' | 'filled';
  signature: Signature;
}

/** A company, as company-generator writes it. */
export interface Company {
  country_code: string;
  name: string;
  /** The name without the legal form. */
  short_name: string;
  /** ISIC Rev. 4 division and its section letter. */
  isic: string;
  isic_label: string;
  isic_section: string;
  size: 'micro' | 'small' | 'medium' | 'large';
  /** Persons employed, the owner counted. */
  employees: number;
  legal_form: string;
  legal_form_code: string;
  legal_form_group: 'sole_proprietorship' | 'partnership' | 'limited_liability' | 'other';
  /** `YYYY-MM-DD`. */
  founded: string;
  numbers: { scheme: string; label: string; value: string; compact: string; reserved: boolean }[];
  domain: string;
  /** The weakest evidence of the statistical fields. */
  evidence: 'measured' | 'modelled' | 'filled';
  signature: Signature;
}

/** One persona: each library's result, as that library writes it. */
export interface Persona {
  name: string;
  sex: Sex;
  age: number;
  /** The city's state or province. */
  admin1: string;
  country: Country;
  city: City;
  birth: Birth;
  biodata: Biodata;
  personality: Personality;
  interest: Interest;
  occupation: Occupation;
  /** The persona's family; present only when the request had `relations`. */
  relatives?: Family;
  /** Only with `identity`; as id-generator writes it. */
  identity?: { country_code: string; birth_date: string; sex: Sex; numbers: Record<string, any>[]; documents: Record<string, any>[]; signature: Signature };
  /** Only with `finance`; as finance-generator writes it. */
  finance?: { country_code: string; birth_date: string; accounts: Record<string, any>[]; cards: Record<string, any>[]; signature: Signature };
  /** Only with `home`; as property-generator writes it. */
  home?: Home;
  /** Only with `home`: the household's gross annual earnings, which the home's cost is drawn from; null when nobody in it earns. */
  household_income?: Money | null;
  /** Only with `employer`: as company-generator writes it, or null when the persona is not employed. */
  employer?: Company | null;
  /** `generate({ ...request, seed: persona.signature.seed })` replays this persona. */
  signature: Signature;
}

export interface PersonaGenerator {
  /** The countries a persona can be generated for (ISO 3166-1 alpha-2, uppercase, sorted). */
  readonly countries: readonly string[];
  /** The most personality traits a request can ask for: the trait catalogue's size. */
  readonly maxTraits: number;
  /**
   * Hash of the data of all eight libraries (16 hex digits). A saved seed replays the same persona
   * only while this is unchanged; it changes when a package update changes any library's data.
   */
  readonly dataHash: string;
  /** id-generator's data hash, combined into the signature of a persona with `identity`. */
  readonly identityDataHash: string;
  /** finance-generator's data hash, combined into the signature of a persona with `finance`. */
  readonly financeDataHash: string;
  /** property-generator's data hash, combined into the signature of a persona with `home`. */
  readonly propertyDataHash: string;
  /** relationship-generator's data hash, combined after property-generator's for a persona with `home`. */
  readonly relationshipDataHash: string;
  /** company-generator's data hash, combined last into the signature of a persona with `employer`. */
  readonly companyDataHash: string;
  /** The library versions (git tags) this module was built from. */
  readonly versions: Readonly<Record<string, string>>;
  /**
   * Generate a persona. Takes about a third of a millisecond.
   * @throws {TypeError} A request field of the wrong type, or an unknown field.
   * @throws {RangeError} A field outside its range: an unsupported country, ages, traits, date or seed.
   */
  generate(request?: PersonaRequest): Persona;
  /** Generate a persona as the JSON text the module returns, exactly as the C++ library writes it. */
  generateJson(request?: PersonaRequest): string;
  /**
   * Generate a batch on this thread. Persona `i` is `generate({ ...request, seed })` for the seed
   * derived from the batch seed and `first + i`, so any persona replays on its own, a range can be
   * generated alone, and two batches that follow each other equal one. For large batches use a
   * pool (`createPersonaPool`), which splits the work across workers with the same result.
   * @throws {TypeError} A field of the wrong type, an unknown field, or a seed inside the request.
   * @throws {RangeError} A count, first index, seed, country or other field out of range.
   */
  generateBatch(options: BatchOptions): Persona[];
  /** The same batch as JSON Lines: one persona per line, each line ending in `\n`. Empty for a count of 0. */
  generateBatchJsonl(options: BatchOptions): string;
}

/** What a batch asks for. */
export interface BatchOptions {
  /** What every persona is generated from (the same fields as `generate`, but no `seed`). */
  request?: Omit<PersonaRequest, 'seed'>;
  /** Number of personas, 0 to 2^31 - 1. */
  count: number;
  /**
   * The batch seed, in the forms `PersonaRequest.seed` takes. Unset draws a random one, so the batch
   * cannot be repeated; each persona's `signature.seed` still replays that persona alone.
   */
  seed?: string | bigint | number;
  /** Index of the first persona, default 0: `first: 1000` starts at persona 1000 of the same batch. */
  first?: number;
}

/** A pool of workers that generates batches in pieces and joins them in order. */
export interface PersonaPool {
  /** Number of workers. */
  readonly size: number;
  /** Same personas as `PersonaGenerator.generateBatch` with the same options, for any pool size. */
  generateBatch(options: BatchOptions): Promise<Persona[]>;
  /** Same text as `PersonaGenerator.generateBatchJsonl`. */
  generateBatchJsonl(options: BatchOptions): Promise<string>;
  /** Stop the workers. The pool cannot be used afterwards. */
  close(): void;
}

export interface PoolOptions {
  /**
   * Number of workers, 1 to 64. Each loads its own copy of the module, about 60 MB, and takes a
   * second or two to start. Default: the number of cores, at most 4.
   */
  workers?: number;
}

/** Emscripten module options this package passes on. */
export interface ModuleOptions {
  /** Map a file the module loads (`persona.wasm`) to its URL. */
  locateFile?: (path: string, prefix: string) => string;
  /** The contents of persona.wasm, to skip fetching it. */
  wasmBinary?: ArrayBuffer | Uint8Array;
  [option: string]: unknown;
}

/**
 * Load the WebAssembly module and its data, and return a generator. Loading takes a second or
 * two and blocks the thread it runs on, so create one generator, reuse it, and in a browser
 * prefer a Web Worker.
 */
export function createPersonaGenerator(moduleOptions?: ModuleOptions): Promise<PersonaGenerator>;

/**
 * Start a pool of Web Workers (worker threads in Node), each with its own module. Resolves when all
 * are ready. Call `close()` when done.
 */
export function createPersonaPool(options?: PoolOptions): Promise<PersonaPool>;
