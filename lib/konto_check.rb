# load the c extension
require 'konto_check_raw'

module KontoCheck

  SEARCH_KEYS = [:ort, :plz, :pz, :bic, :blz, :namen, :namen_kurz]

  SEARCH_KEY_MAPPINGS = {
    :city       => :ort,
    :zip        => :plz,
    :name       => :namen,
    :kurzname   => :namen_kurz,
    :shortname  => :namen_kurz
  }

  class << self

# die Funktionalität von lut_info() aus KontoCheckRaw ist auf drei Funktionen verteilt:
# lut_info() gibt die Infos zum geladenen Datensatz zurück, die Funktionen
# lut_info1(<filename>) und lut_info2(<filename>) die Infos zum ersten bzw.
# zweiten Datensatz der Datei <filename>. Alle drei Funktionen liefern nur den Klartext.

    def lut_info()
      KontoCheckRaw::lut_info()[3]
    end

    def lut_info1(filename)
      KontoCheckRaw::lut_info(filename)[3]
    end

    def lut_info2(filename)
      KontoCheckRaw::lut_info(filename)[4]
    end

    def dump_lutfile(filename)
      KontoCheckRaw::dump_lutfile(filename).first
    end

    def retval2dos(*args)
      KontoCheckRaw::retval2dos(*args)
    end

    def retval2html(*args)
      KontoCheckRaw::retval2html(*args)
    end

    def retval2utf8(*args)
      KontoCheckRaw::retval2utf8(*args)
    end

    def retval2txt(*args)
      KontoCheckRaw::retval2txt(*args)
    end

    def generate_lutfile(*args)
      KontoCheckRaw::generate_lutfile(*args)
    end

    def init(*args)
      KontoCheckRaw::init(*args)
    end

    def free(*args)
      KontoCheckRaw::free(*args)
    end

    def konto_check(blz, ktno)
      KontoCheckRaw::konto_check(blz, ktno)[1]
    end
    alias_method :valid, :konto_check

    def konto_check?(blz, ktno)
      KontoCheckRaw::konto_check(blz, ktno).first
    end
    alias_method :valid?, :konto_check?

    def bank_valid(*args)
      KontoCheckRaw::bank_valid(*args)[1]
    end

    def bank_valid?(*args)
      KontoCheckRaw::bank_valid(*args).first
    end

    def bank_filialen(*args)
      KontoCheckRaw::bank_filialen(*args).first
    end

    def bank_name(*args)
      KontoCheckRaw::bank_name(*args).first
    end

    def bank_name_kurz(*args)
      KontoCheckRaw::bank_name_kurz(*args).first
    end

    def bank_ort(*args)
      KontoCheckRaw::bank_ort(*args).first
    end

    def bank_plz(*args)
      KontoCheckRaw::bank_plz(*args).first
    end

    def bank_pz(*args)
      KontoCheckRaw::bank_pz(*args).first
    end

    def bank_bic(*args)
      KontoCheckRaw::bank_bic(*args).first
    end

    def bank_aenderung(*args)
      KontoCheckRaw::bank_aenderung(*args).first
    end

    def bank_loeschung(*args)
      KontoCheckRaw::bank_loeschung(*args).first
    end

    def bank_nachfolge_blz(*args)
      KontoCheckRaw::bank_nachfolge_blz(*args).first
    end

    def bank_pan(*args)
      KontoCheckRaw::bank_pan(*args).first
    end

    def bank_nr(*args)
      KontoCheckRaw::bank_nr(*args).first
    end

# Mammutfunktion, die alle Infos über eine Bank zurückliefert
    def bank_alles(*args)
      KontoCheckRaw::bank_alles(*args)
    end

    def suche(options={})
      key   = options.keys.first.to_sym
      value = options[key]
      key = SEARCH_KEY_MAPPINGS[key] if SEARCH_KEY_MAPPINGS.keys.include?(key)
      raise 'search key not supported' unless SEARCH_KEYS.include?(key)
      raw_results = KontoCheckRaw::send("bank_suche_#{key}", value)
#      puts "RESULT: #{raw_results.inspect}"
#      []
      raw_results[1]
    end
    alias_method :search, :suche

  end

end