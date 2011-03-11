module KontoCheck

  def self.konto_check?(blz, ktno)
    KontoCheck::konto_check(blz, ktno).first
  end

end