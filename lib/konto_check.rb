# load the c extension
require 'konto_check_raw'

module KontoCheck

  SEARCH_KEYS = [:ort, :plz, :blz, :namen, :namen_kurz]
  SEARCH_KEY_MAPPINGS = {
    :city       => :ort,
    :zip        => :plz,
    :name       => :namen,
    :kurzname   => :namen_kurz,
    :shortname  => :namen_kurz
  }

  class << self

    def generate_lutfile(*args)
      KontoCheckRaw::generate_lutfile(*args)
    end

    def init(*args)
      KontoCheckRaw::init(*args)
    end

    def konto_check?(blz, ktno)
      KontoCheckRaw::konto_check(blz, ktno).first
    end
    alias_method :valid?, :konto_check?

    def bank_name(blz)
      KontoCheckRaw::bank_name(blz).first
    end

    def suche(options={})
      key   = options.keys.first.to_sym
      value = options[key]
      key = SEARCH_KEY_MAPPINGS[key] if SEARCH_KEY_MAPPINGS.keys.include?(key)
      raise 'search key not supported' unless SEARCH_KEYS.include?(key)
      raw_results = KontoCheckRaw::send("bank_suche_#{key}", value)
      puts "RESULT: #{raw_results.inspect}"
      []
    end
    alias_method :search, :suche

  end

end