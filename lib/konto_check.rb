# load the c extension
require 'konto_check_c'

module KontoCheck

  def self.konto_check?(blz, ktno)
    KontoCheck::konto_check(blz, ktno).first
  end

end