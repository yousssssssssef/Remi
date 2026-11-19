import { useState } from 'react';
import { AppText, Button, Card } from '@/components/ui/core';
import { FormField } from '@/components/ui/form-field';
import { kitchenRequest } from '@/services/kitchen-api';
import { ConnectedScreen, Feedback, useAction, useCopy } from './connected-ui';

const pairDevice = 'mutation PairDevice($code: String!) { pairDevice(code: $code) { deviceName } }';

/** Links a Remi kitchen device to the active household: the device shows a 6-digit code, the person types it here. */
export function ConnectedRemiDeviceScreen() {
  const copy = useCopy(); const action = useAction(); const [code, setCode] = useState('');
  const digits = code.replace(/\D/g, '').slice(0, 6);
  const pair = () => { void action.run(async () => { await kitchenRequest(pairDevice, { code: digits }); setCode(''); }, copy('Paired. Remi shows your kitchen in a few seconds.', 'تم الاقتران. سيعرض Remi مطبخك خلال ثوانٍ.'), 'success'); };
  return <ConnectedScreen title={copy('Pair a Remi device', 'اقتران جهاز Remi')}>
    <AppText>{copy('Remi on your fridge shows a 6-digit code on its Connect page when it is not linked yet. Type it here to link it to this household.', 'يعرض جهاز Remi على ثلاجتك رمزًا من 6 أرقام في صفحة الاتصال عندما لا يكون مرتبطًا. اكتبه هنا لربطه بهذه الأسرة.')}</AppText>
    <Card>
      <FormField label={copy('Code on Remi', 'الرمز على Remi')} value={digits} onChangeText={setCode} keyboardType="number-pad" maxLength={6} autoComplete="one-time-code" editable={!action.busy} />
      <Feedback message={action.message} />
      <Button label={copy('Pair device', 'اقتران الجهاز')} disabled={action.busy || digits.length !== 6} onPress={pair} />
      <AppText variant="caption" muted>{copy('The device gets its own sign-in for this household. Sign it out any time from your account sessions.', 'يحصل الجهاز على تسجيل دخول خاص به لهذه الأسرة. يمكنك إنهاؤه في أي وقت من جلسات حسابك.')}</AppText>
    </Card>
  </ConnectedScreen>;
}
